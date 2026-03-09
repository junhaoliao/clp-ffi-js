import {
    afterEach,
    beforeAll,
    beforeEach,
    describe,
    expect,
    it,
} from "vitest";

import type {
    ClpSfaReader,
    MainModule,
} from "../dist/ClpFfiJs-node.js";

import {
    createModule,
    loadTestData,
} from "./utils.js";


const SFA_MULTI_TEST_FILE = "sfa-multi.clp";

let module: MainModule;
let sfaData: Uint8Array;
let reader: ClpSfaReader;

beforeAll(async () => {
    module = await createModule();
    sfaData = await loadTestData(SFA_MULTI_TEST_FILE);
});

beforeEach(() => {
    reader = new module.ClpSfaReader(sfaData);
});

afterEach(() => {
    reader.delete();
});

describe("SFA Multi-File Basics", () => {
    it("should open multi-file archive and report non-zero event count", () => {
        const numEvents = reader.getNumEvents();

        expect(numEvents).toBeGreaterThan(0);
    });

    it("should return metadata with multiple files", () => {
        const metadata = reader.getMetadata();

        expect(metadata.files).toBeInstanceOf(Array);
        expect(metadata.files.length).toBeGreaterThan(1);
    });

    it("should have file entries with non-overlapping index ranges", () => {
        const metadata = reader.getMetadata();
        const files = metadata.files;

        // Sort by startIndex
        const sorted = [...files].sort(
            (a: {startIndex: number}, b: {startIndex: number}) =>
                a.startIndex - b.startIndex
        );

        for (let i = 1; i < sorted.length; i++) {
            const prev = sorted[i - 1];
            const curr = sorted[i];

            if (undefined === prev || undefined === curr) {
                continue;
            }

            expect(curr.startIndex).toBeGreaterThanOrEqual(prev.endIndex);
        }
    });

    it("should have file entries with distinct _filename fields", () => {
        const metadata = reader.getMetadata();
        const filenames = new Set<string>();

        for (const file of metadata.files) {
            const name = String(file.fields["_filename"]);
            expect(name.length).toBeGreaterThan(0);
            filenames.add(name);
        }

        expect(filenames.size).toBe(metadata.files.length);
    });

    it("should have total event count equal to sum of all file ranges", () => {
        const metadata = reader.getMetadata();
        let sum = 0;

        for (const file of metadata.files) {
            sum += file.endIndex - file.startIndex;
        }

        expect(sum).toBe(metadata.totalLogEventCount);
    });
});

describe("SFA Multi-File getFileNames", () => {
    it("should return an array of distinct file names", () => {
        const fileNames = reader.getFileNames();

        expect(fileNames).toBeInstanceOf(Array);
        expect(fileNames.length).toBeGreaterThan(1);
        expect(new Set(fileNames).size).toBe(fileNames.length);
    });

    it("should match _filename fields from metadata", () => {
        const fileNames = reader.getFileNames();
        const metadata = reader.getMetadata();

        expect(fileNames.length).toBe(metadata.files.length);
        for (let i = 0; i < fileNames.length; i++) {
            expect(fileNames[i]).toBe(String(metadata.files[i]?.fields["_filename"]));
        }
    });

    it("should return names usable with decodeRangeByFile", () => {
        const fileNames = reader.getFileNames();

        for (const name of fileNames) {
            const results = reader.decodeRangeByFile(name, 0, 1, false);
            expect(results).not.toBeNull();
            if (null !== results) {
                expect(results.length).toBe(1);
            }
        }
    });
});

describe("SFA Multi-File decodeRange", () => {
    it("should decode events from the first file's range", () => {
        const metadata = reader.getMetadata();
        const firstFile = metadata.files[0];

        expect(firstFile).toBeDefined();
        if (undefined === firstFile) {
            return;
        }

        const end = Math.min(firstFile.startIndex + 10, firstFile.endIndex);
        const results = reader.decodeRange(firstFile.startIndex, end, false);

        expect(results).not.toBeNull();
        if (null !== results) {
            expect(results.length).toBe(end - firstFile.startIndex);
        }
    });

    it("should decode events from the last file's range", () => {
        const metadata = reader.getMetadata();
        const lastFile = metadata.files[metadata.files.length - 1];

        expect(lastFile).toBeDefined();
        if (undefined === lastFile) {
            return;
        }

        const start = Math.max(lastFile.endIndex - 10, lastFile.startIndex);
        const results = reader.decodeRange(start, lastFile.endIndex, false);

        expect(results).not.toBeNull();
        if (null !== results) {
            expect(results.length).toBe(lastFile.endIndex - start);
        }
    });

    it("should decode events spanning multiple files", () => {
        const metadata = reader.getMetadata();

        expect(metadata.files.length).toBeGreaterThan(1);

        const firstFile = metadata.files[0];
        const secondFile = metadata.files[1];

        expect(firstFile).toBeDefined();
        expect(secondFile).toBeDefined();
        if (undefined === firstFile || undefined === secondFile) {
            return;
        }

        // Decode a range that spans across two files
        const begin = Math.max(firstFile.endIndex - 5, firstFile.startIndex);
        const end = Math.min(secondFile.startIndex + 5, secondFile.endIndex);
        const results = reader.decodeRange(begin, end, false);

        expect(results).not.toBeNull();
        if (null !== results) {
            expect(results.length).toBe(end - begin);
        }
    });

    it("should return monotonically increasing logEventIdx across all files", () => {
        const numEvents = reader.getNumEvents();
        const batchSize = 1000;
        let prevIdx = -1;

        for (let offset = 0; offset < numEvents; offset += batchSize) {
            const end = Math.min(offset + batchSize, numEvents);
            const results = reader.decodeRange(offset, end, false);

            expect(results).not.toBeNull();
            if (null !== results) {
                for (const event of results) {
                    expect(event.logEventIdx).toBeGreaterThan(prevIdx);
                    prevIdx = event.logEventIdx;
                }
            }
        }
    });
});

describe("SFA Multi-File decodeRangeByFile", () => {
    it("should decode first N events from a specific file by name", () => {
        const metadata = reader.getMetadata();
        const secondFile = metadata.files[1];

        expect(secondFile).toBeDefined();
        if (undefined === secondFile) {
            return;
        }

        const filename = String(secondFile.fields["_filename"]);
        const count = Math.min(5, secondFile.endIndex - secondFile.startIndex);
        const results = reader.decodeRangeByFile(filename, 0, count, false);

        expect(results).not.toBeNull();
        if (null !== results) {
            expect(results.length).toBe(count);

            // The returned logEventIdx values should fall within the file's global range
            for (const event of results) {
                expect(event.logEventIdx).toBeGreaterThanOrEqual(secondFile.startIndex);
                expect(event.logEventIdx).toBeLessThan(secondFile.endIndex);
            }
        }
    });

    it("should return all events from a file when range covers the whole file", () => {
        const metadata = reader.getMetadata();
        const secondFile = metadata.files[1];

        expect(secondFile).toBeDefined();
        if (undefined === secondFile) {
            return;
        }

        const filename = String(secondFile.fields["_filename"]);
        const fileEventCount = secondFile.endIndex - secondFile.startIndex;
        const results = reader.decodeRangeByFile(filename, 0, fileEventCount, false);

        expect(results).not.toBeNull();
        if (null !== results) {
            expect(results.length).toBe(fileEventCount);
        }
    });

    it("should return null for out-of-bounds file-relative range", () => {
        const metadata = reader.getMetadata();
        const secondFile = metadata.files[1];

        expect(secondFile).toBeDefined();
        if (undefined === secondFile) {
            return;
        }

        const filename = String(secondFile.fields["_filename"]);
        const fileEventCount = secondFile.endIndex - secondFile.startIndex;
        const results = reader.decodeRangeByFile(filename, 0, fileEventCount + 100, false);

        expect(results).toBeNull();
    });

    it("should return null for unknown filename", () => {
        const results = reader.decodeRangeByFile("nonexistent_file.log", 0, 10, false);

        expect(results).toBeNull();
    });

    it("should produce same results as manual global-index decodeRange", () => {
        const metadata = reader.getMetadata();
        const secondFile = metadata.files[1];

        expect(secondFile).toBeDefined();
        if (undefined === secondFile) {
            return;
        }

        const filename = String(secondFile.fields["_filename"]);
        const count = Math.min(5, secondFile.endIndex - secondFile.startIndex);

        const byFile = reader.decodeRangeByFile(filename, 0, count, false);
        const byGlobal = reader.decodeRange(secondFile.startIndex, secondFile.startIndex + count, false);

        expect(byFile).not.toBeNull();
        expect(byGlobal).not.toBeNull();
        if (null !== byFile && null !== byGlobal) {
            expect(byFile.length).toBe(byGlobal.length);
            for (let i = 0; i < byFile.length; i++) {
                expect(byFile[i]?.logEventIdx).toBe(byGlobal[i]?.logEventIdx);
                expect(byFile[i]?.timestamp).toBe(byGlobal[i]?.timestamp);
                expect(byFile[i]?.message).toBe(byGlobal[i]?.message);
            }
        }
    });
});

describe("SFA Multi-File Query Filter", () => {
    it("should filter events with KQL query across multiple files", () => {
        reader.setQuery("*");

        const results = reader.decodeRange(0, 1, true);
        expect(results).not.toBeNull();

        const filteredCount = reader.getNumFilteredEvents();
        expect(filteredCount).toBeGreaterThan(0);
    });
});
