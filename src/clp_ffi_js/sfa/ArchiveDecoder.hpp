#ifndef CLP_FFI_JS_SFA_ARCHIVEDECODER_HPP
#define CLP_FFI_JS_SFA_ARCHIVEDECODER_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include <clp_s/ArchiveReader.hpp>
#include <clp_s/SchemaReader.hpp>
#include <clp_s/search/QueryRunner.hpp>
#include <clp_s/search/SchemaMatch.hpp>
#include <clp_s/search/ast/Expression.hpp>
#include <emscripten/val.h>
#include <ystdlib/containers/Array.hpp>

namespace clp_ffi_js::sfa {
EMSCRIPTEN_DECLARE_VAL_TYPE(DataArrayTsType);
EMSCRIPTEN_DECLARE_VAL_TYPE(DecodedResultsTsType);
EMSCRIPTEN_DECLARE_VAL_TYPE(FileNamesTsType);
EMSCRIPTEN_DECLARE_VAL_TYPE(MetadataTsType);

class ArchiveDecoder {
public:
    /**
     * Creates an ArchiveDecoder from the given data array.
     * @param data_array A Uint8Array containing a CLP-S single-file archive.
     * @return The created instance.
     * @throw ClpFfiJsException on error.
     */
    [[nodiscard]] static auto create(DataArrayTsType const& data_array)
            -> std::unique_ptr<ArchiveDecoder>;

    ~ArchiveDecoder();

    // Disable copy/move
    ArchiveDecoder(ArchiveDecoder const&) = delete;
    auto operator=(ArchiveDecoder const&) -> ArchiveDecoder& = delete;
    ArchiveDecoder(ArchiveDecoder&&) = delete;
    auto operator=(ArchiveDecoder&&) -> ArchiveDecoder& = delete;

    /**
     * @return Metadata about the archive as a JavaScript object.
     */
    [[nodiscard]] auto get_metadata() const -> MetadataTsType;

    /**
     * @return An array of source file names from the archive's range index.
     */
    [[nodiscard]] auto get_file_names() const -> FileNamesTsType;

    /**
     * @return The total number of log events in the archive.
     */
    [[nodiscard]] auto get_num_events() const -> size_t { return m_total_num_events; }

    /**
     * Decodes log events in the range [begin_idx, end_idx) and returns them as a JS array.
     * @param begin_idx
     * @param end_idx
     * @param use_filter Whether to apply the active KQL filter.
     * @return An array of decoded log event objects, or null on error.
     */
    [[nodiscard]] auto decode_range(size_t begin_idx, size_t end_idx, bool use_filter)
            -> DecodedResultsTsType;

    /**
     * Decodes log events in [begin_idx, end_idx) relative to a specific file within the archive.
     * @param file_name The `_filename` field value identifying the source file.
     * @param begin_idx Start index relative to the file (inclusive, 0-based within the file).
     * @param end_idx End index relative to the file (exclusive, 0-based within the file).
     * @param use_filter Whether to apply the active KQL filter.
     * @return An array of decoded log event objects, or null if the file is not found or the range
     * is out of bounds.
     */
    [[nodiscard]] auto decode_range_by_file(
            std::string const& file_name,
            size_t begin_idx,
            size_t end_idx,
            bool use_filter
    ) -> DecodedResultsTsType;

    /**
     * Sets a KQL query filter. Subsequent decode_range calls with use_filter=true will only return
     * matching log events.
     * @param kql_query The KQL query string.
     */
    void set_query(std::string const& kql_query);

    /**
     * Clears any active KQL query filter.
     */
    void clear_query();

    /**
     * @return The number of filtered log events (after KQL filter), or the total count if no filter
     * is active.
     */
    [[nodiscard]] auto get_num_filtered_events() const -> size_t;

private:
    struct LogEventData {
        int64_t log_event_idx;
        clp_s::epochtime_t timestamp;
        std::string message;
    };

    ArchiveDecoder(
            ystdlib::containers::Array<char> data_buffer,
            std::shared_ptr<clp_s::ArchiveReader> archive_reader,
            size_t total_num_events
    );

    /**
     * Opens a fresh ArchiveReader from the stored buffer.
     * @return A shared pointer to the newly opened ArchiveReader.
     */
    [[nodiscard]] auto open_archive_from_buffer() -> std::shared_ptr<clp_s::ArchiveReader>;

    /**
     * Reads all log events from the archive in order, applying any active filter.
     * @return A vector of LogEventData.
     */
    [[nodiscard]] auto read_all_events_in_order() -> std::vector<LogEventData>;

    /**
     * Reads all log events from the archive in order without filtering.
     * @return A vector of LogEventData.
     */
    [[nodiscard]] auto read_all_events_unfiltered() -> std::vector<LogEventData>;

    /**
     * Reads all log events matching the current query filter.
     * @return A vector of LogEventData.
     */
    [[nodiscard]] auto read_filtered_events() -> std::vector<LogEventData>;

    ystdlib::containers::Array<char> m_data_buffer;
    std::shared_ptr<clp_s::ArchiveReader> m_archive_reader;
    size_t m_total_num_events;

    // Query state
    std::shared_ptr<clp_s::search::ast::Expression> m_query_expr;
    std::shared_ptr<clp_s::search::SchemaMatch> m_schema_match;

    // Cached filtered events
    bool m_filter_dirty{true};
    std::vector<LogEventData> m_cached_all_events;
    std::vector<LogEventData> m_cached_filtered_events;
    bool m_all_events_cached{false};
};
}  // namespace clp_ffi_js::sfa

#endif  // CLP_FFI_JS_SFA_ARCHIVEDECODER_HPP
