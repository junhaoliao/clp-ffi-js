import type {ClpSfaReader, MainModule} from "../../../dist/ClpFfiJs-node.js";

import type {ClpQuery} from "./KqlQuery.js";
import {LogEvent} from "./LogEvent.js";
import type {ArchiveMetadata, FileInfo, ReaderOptions} from "./types.js";


// FinalizationRegistry safety net for WASM cleanup.
const PREVENT_LEAK = new FinalizationRegistry<ClpSfaReader>((native) => {
    if (false === native.isDeleted()) {
        console.warn("ClpArchiveReader was not closed; releasing WASM object via FinalizationRegistry");
        native.delete();
    }
});

/**
 * Read and iterate log events from a CLP-S single-file archive.
 *
 * Accepts ArrayBuffer, Uint8Array, or Blob. The entire archive must be in
 * memory before decoding can begin (the SFA format requires random access).
 */
class ClpArchiveReader {
    readonly #native: ClpSfaReader;

    readonly #metadata: ArchiveMetadata;

    #closed: boolean = false;

    private constructor(native: ClpSfaReader, _options: ReaderOptions) {
        this.#native = native;

        // Extract metadata from native object.
        const rawMeta = native.getMetadata();
        const files: FileInfo[] = rawMeta.files.map((f: {startIndex: number; endIndex: number; fields: Record<string, string | number>}) => ({
            path: ("_filename" in f.fields) ? String(f.fields["_filename"]) : "",
            logEventIdxStart: f.startIndex,
            logEventIdxEnd: f.endIndex,
            fields: f.fields,
        }));

        this.#metadata = {
            totalLogEventCount: rawMeta.totalLogEventCount,
            files: files,
        };

        PREVENT_LEAK.register(this, native, this);
    }

    /**
     * Open a CLP archive for reading.
     *
     * @param source Archive data. Must be fully in memory.
     * @param options Reader options.
     * @returns A reader ready for iteration.
     */
    static async open(
        source: ArrayBuffer | Uint8Array | Blob,
        options: ReaderOptions = {}
    ): Promise<ClpArchiveReader> {
        const module = await ClpArchiveReader.#loadModule();

        let data: Uint8Array;
        if (source instanceof Blob) {
            const buffer = await source.arrayBuffer();
            data = new Uint8Array(buffer);
        } else if (source instanceof ArrayBuffer) {
            data = new Uint8Array(source);
        } else {
            data = source;
        }

        const native = new module.ClpSfaReader(data);

        return new ClpArchiveReader(native, options);
    }

    /**
     * Node.js convenience -- reads the file into an ArrayBuffer internally.
     *
     * @param path File system path to the archive.
     * @param options Reader options.
     */
    static async fromFile(
        path: string,
        options: ReaderOptions = {}
    ): Promise<ClpArchiveReader> {
        // Dynamic import for Node.js APIs.
        // eslint-disable-next-line no-inline-comments
        const fs = await import(/* @vite-ignore */ "node:fs");
        const data = new Uint8Array(fs.readFileSync(path));

        return ClpArchiveReader.open(data, options);
    }

    /**
     * Releases the underlying WASM/Embind object.
     */
    close(): void {
        if (this.#closed) {
            return;
        }
        this.#closed = true;
        PREVENT_LEAK.unregister(this);
        this.#native.delete();
    }

    /**
     * Archive metadata, available after open resolves.
     */
    get metadata(): ArchiveMetadata {
        return this.#metadata;
    }

    /**
     * Set a query for filter pushdown used by decodeRange.
     *
     * @param query Search query.
     */
    setQuery(query: ClpQuery): void {
        this.#assertNotClosed();
        this.#native.setQuery(query.getQueryString());
    }

    /**
     * Clear any active query filter.
     */
    clearQuery(): void {
        this.#assertNotClosed();
        this.#native.clearQuery();
    }

    /**
     * Returns the number of events matching the current query filter,
     * or the total count if no filter is active.
     */
    getNumFilteredEvents(): number {
        this.#assertNotClosed();

        return this.#native.getNumFilteredEvents();
    }

    /**
     * Decode log events in [beginIdx, endIdx) by global log_event_idx.
     *
     * @param beginIdx Start index (inclusive).
     * @param endIdx End index (exclusive).
     * @param useQuery Whether to apply the query set via setQuery.
     * @returns Array of LogEvent objects, or null on error.
     */
    decodeRange(
        beginIdx: number,
        endIdx: number,
        useQuery: boolean = false
    ): LogEvent[] | null {
        this.#assertNotClosed();

        const rawResults = this.#native.decodeRange(beginIdx, endIdx, useQuery);
        if (null === rawResults) {
            return null;
        }

        return rawResults.map(
            (r: {logEventIdx: number; timestamp: number; message: string}) => new LogEvent(r.logEventIdx, r.timestamp, r.message)
        );
    }

    /**
     * Same as decodeRange but indices are relative to a file (0-based within
     * the file). Internally offsets by file.logEventIdxStart.
     *
     * @param fileName Source file name (from metadata.files).
     * @param beginIdx Start index relative to file (inclusive).
     * @param endIdx End index relative to file (exclusive).
     * @param useQuery Whether to apply the query set via setQuery.
     * @returns Array of LogEvent objects, or null on error.
     */
    decodeRangeByFile(
        fileName: string,
        beginIdx: number,
        endIdx: number,
        useQuery: boolean = false
    ): LogEvent[] | null {
        this.#assertNotClosed();

        const file = this.#metadata.files.find((f) => f.path === fileName);
        if (undefined === file) {
            throw new Error(`File not found in archive: ${fileName}`);
        }

        const globalBegin = file.logEventIdxStart + beginIdx;
        const globalEnd = file.logEventIdxStart + endIdx;

        return this.decodeRange(globalBegin, globalEnd, useQuery);
    }

    /**
     * Stream all log events as a ReadableStream.
     *
     * @param query Optional query for filter pushdown.
     * @returns A ReadableStream of LogEvent objects.
     */
    stream(query?: ClpQuery): ReadableStream<LogEvent> {
        this.#assertNotClosed();

        if (undefined !== query) {
            this.setQuery(query);
        }

        const useFilter = undefined !== query;
        const numEvents = useFilter ?
            this.getNumFilteredEvents() :
            this.#metadata.totalLogEventCount;

        const batchSize = 1000;
        let offset = 0;

        // eslint-disable-next-line @typescript-eslint/no-this-alias
        const reader = this;

        return new ReadableStream<LogEvent>({
            pull(controller) {
                if (offset >= numEvents) {
                    controller.close();

                    return;
                }

                const end = Math.min(offset + batchSize, numEvents);
                const events = reader.decodeRange(offset, end, useFilter);

                if (null === events || 0 === events.length) {
                    controller.close();

                    return;
                }

                for (const event of events) {
                    controller.enqueue(event);
                }

                offset = end;
            },
        });
    }

    static #module: MainModule | null = null;

    static async #loadModule(): Promise<MainModule> {
        if (null !== ClpArchiveReader.#module) {
            return ClpArchiveReader.#module;
        }

        // eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
        const isNode = "undefined" !== typeof process && "string" === typeof process.versions?.node;

        if (isNode) {
            const {default: factory} =
                // eslint-disable-next-line no-inline-comments
                await import(/* @vite-ignore */ "../../../dist/ClpFfiJs-node.js");

            ClpArchiveReader.#module = await factory();
        } else {
            const {default: factory} = await import("../../../dist/ClpFfiJs-worker.js");
            ClpArchiveReader.#module = await factory();
        }

        return ClpArchiveReader.#module;
    }

    #assertNotClosed(): void {
        if (this.#closed) {
            throw new Error("ClpArchiveReader has been closed");
        }
    }
}

export {ClpArchiveReader};
