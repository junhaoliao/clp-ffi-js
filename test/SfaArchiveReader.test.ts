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


const SFA_TEST_FILE = "sfa.clp";

let module: MainModule;
let sfaData: Uint8Array;
let reader: ClpSfaReader;

beforeAll(async () => {
    module = await createModule();
    sfaData = await loadTestData(SFA_TEST_FILE);
});

beforeEach(() => {
    reader = new module.ClpSfaReader(sfaData);
});

afterEach(() => {
    reader.delete();
});

describe("SFA Reader Basics", () => {
    it("should open archive and report non-zero event count", () => {
        const numEvents = reader.getNumEvents();

        expect(numEvents).toBeGreaterThan(0);
    });

    it("should return metadata with totalLogEventCount > 0", () => {
        const metadata = reader.getMetadata();

        expect(metadata).not.toBeNull();
        expect(metadata.totalLogEventCount).toBeGreaterThan(0);
        expect(metadata.totalLogEventCount).toBe(reader.getNumEvents());
    });

    it("should return metadata with non-empty files array", () => {
        const metadata = reader.getMetadata();

        expect(metadata.files).toBeInstanceOf(Array);
        expect(metadata.files.length).toBeGreaterThan(0);
    });

    it("should have file entries with valid start/end indices", () => {
        const metadata = reader.getMetadata();

        for (const file of metadata.files) {
            expect(file.startIndex).toBeGreaterThanOrEqual(0);
            expect(file.endIndex).toBeGreaterThan(file.startIndex);
            expect(file.fields).toBeDefined();
        }
    });
});

describe("SFA Reader decodeRange", () => {
    it("should decode a range of events", () => {
        const numEvents = reader.getNumEvents();
        const end = Math.min(10, numEvents);
        const results = reader.decodeRange(0, end, false);

        expect(results).not.toBeNull();
        if (null !== results) {
            expect(results.length).toBe(end);
        }
    });

    it("should return events with expected fields", () => {
        const results = reader.decodeRange(0, 1, false);

        expect(results).not.toBeNull();
        if (null !== results) {
            expect(results.length).toBe(1);
            const event = results[0];
            expect(event).toBeDefined();
            if (undefined !== event) {
                expect(typeof event.logEventIdx).toBe("number");
                expect(typeof event.timestamp).toBe("number");
                expect(typeof event.message).toBe("string");
                expect(event.message.length).toBeGreaterThan(0);
            }
        }
    });

    it("should return monotonically increasing logEventIdx when iterating all events", () => {
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

    it("should handle empty range", () => {
        const results = reader.decodeRange(0, 0, false);

        expect(results).not.toBeNull();
        if (null !== results) {
            expect(results.length).toBe(0);
        }
    });

    it("should return null for out-of-bounds range", () => {
        const numEvents = reader.getNumEvents();
        const results = reader.decodeRange(0, numEvents + 100, false);

        expect(results).toBeNull();
    });

    it("should return null for invalid range (begin > end)", () => {
        const results = reader.decodeRange(10, 5, false);

        expect(results).toBeNull();
    });
});

describe("SFA Reader Query Filter", () => {
    it("should parse and apply a KQL query without throwing", () => {
        expect(() => {
            reader.setQuery("*");
        }).not.toThrow();
    });

    it("should return filtered results with use_filter=true", () => {
        reader.setQuery("*");

        // Need to call decodeRange first to populate cache
        const results = reader.decodeRange(0, 1, true);
        expect(results).not.toBeNull();

        const filteredCount = reader.getNumFilteredEvents();
        expect(filteredCount).toBeGreaterThan(0);
    });

    it("should clear query", () => {
        reader.setQuery("*");
        reader.clearQuery();
        const numFiltered = reader.getNumFilteredEvents();

        expect(numFiltered).toBe(reader.getNumEvents());
    });

    it("should throw on invalid KQL query", () => {
        expect(() => {
            reader.setQuery("AND AND AND");
        }).toThrow();
    });
});
