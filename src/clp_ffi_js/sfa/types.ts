/**
 * Typed field value within a log event's key-value pairs.
 */
type FieldValue =
    | number
    | string
    | boolean
    | null
    | {[key: string]: FieldValue}
    | FieldValue[];

/**
 * A field name and its type within a schema.
 */
interface FieldInfo {
    name: string;
    type: string;
}

/**
 * Source file metadata from the archive's range index.
 */
interface FileInfo {
    path: string;
    logEventIdxStart: number;
    logEventIdxEnd: number;
    fields: Record<string, string | number>;
}

/**
 * Schema metadata describing a group of records with identical field sets.
 */
interface SchemaInfo {
    schemaId: number;
    logEventCount: number;
    fields: FieldInfo[];
}

/**
 * Top-level archive metadata, available after opening an archive.
 */
interface ArchiveMetadata {
    totalLogEventCount: number;
    files: FileInfo[];
}

/**
 * Warning codes for recoverable issues.
 */
type WarningCode =
    | "RECORD_CORRUPT"
    | "DICT_REF_MISSING"
    | "STREAM_CORRUPT"
    | "SCHEMA_TABLE_SKIPPED";

/**
 * Options for opening an archive reader.
 */
interface ReaderOptions {
    strict?: boolean;
    onwarn?: (code: WarningCode, message: string) => void;
}

export type {
    ArchiveMetadata,
    FieldInfo,
    FieldValue,
    FileInfo,
    ReaderOptions,
    SchemaInfo,
    WarningCode,
};
