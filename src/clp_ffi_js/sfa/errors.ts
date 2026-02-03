/**
 * Bad magic number or unsupported archive version.
 */
class InvalidArchiveError extends Error {
    constructor(message: string) {
        super(message);
        this.name = "InvalidArchiveError";
    }
}

/**
 * Schema tree, timestamp dictionary, or range index is corrupt.
 */
class MetadataCorruptError extends Error {
    constructor(message: string) {
        super(message);
        this.name = "MetadataCorruptError";
    }
}

/**
 * Zstd decompression failed on a metadata section.
 */
class DecompressionError extends Error {
    constructor(message: string) {
        super(message);
        this.name = "DecompressionError";
    }
}

/**
 * I/O failure reading archive data.
 */
class ArchiveIOError extends Error {
    constructor(message: string) {
        super(message);
        this.name = "ArchiveIOError";
    }
}

export {
    ArchiveIOError,
    DecompressionError,
    InvalidArchiveError,
    MetadataCorruptError,
};
