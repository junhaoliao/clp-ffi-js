#include "ArchiveDecoder.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <queue>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <clp/BufferReader.hpp>
#include <clp/ErrorCode.hpp>
#include <clp_s/ArchiveReader.hpp>
#include <clp_s/SchemaReader.hpp>
#include <clp_s/search/QueryRunner.hpp>
#include <clp_s/search/SchemaMatch.hpp>
#include <clp_s/search/ast/EmptyExpr.hpp>
#include <clp_s/search/kql/kql.hpp>
#include <emscripten/em_asm.h>
#include <emscripten/val.h>
#include <ystdlib/containers/Array.hpp>

#include <clp_ffi_js/ClpFfiJsException.hpp>

namespace clp_ffi_js::sfa {
auto ArchiveDecoder::create(DataArrayTsType const& data_array)
        -> std::unique_ptr<ArchiveDecoder> {
    auto const length{data_array["length"].as<size_t>()};

    // Copy array from JavaScript to C++
    ystdlib::containers::Array<char> data_buffer(length);
    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
    emscripten::val::module_property("HEAPU8")
            .call<void>("set", data_array, reinterpret_cast<uintptr_t>(data_buffer.data()));
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)

    // Create a BufferReader from the data and open the archive
    auto buffer_reader = std::make_shared<clp::BufferReader>(data_buffer.data(), length);
    auto archive_reader = std::make_shared<clp_s::ArchiveReader>();
    try {
        archive_reader->open(buffer_reader, "sfa_archive");
    } catch (std::exception const& e) {
        throw ClpFfiJsException{
                clp::ErrorCode::ErrorCode_Failure,
                __FILENAME__,
                __LINE__,
                std::format("Failed to open CLP-S archive (open): {}", e.what())
        };
    }
    try {
        archive_reader->read_dictionaries_and_metadata();
    } catch (std::exception const& e) {
        throw ClpFfiJsException{
                clp::ErrorCode::ErrorCode_Failure,
                __FILENAME__,
                __LINE__,
                std::format("Failed to open CLP-S archive (metadata): {}", e.what())
        };
    }
    try {
        archive_reader->open_packed_streams();
    } catch (std::exception const& e) {
        throw ClpFfiJsException{
                clp::ErrorCode::ErrorCode_Failure,
                __FILENAME__,
                __LINE__,
                std::format("Failed to open CLP-S archive (packed_streams): {}", e.what())
        };
    }

    // Count total events across all schemas
    size_t total_num_events{0};
    try {
        auto tables = archive_reader->read_all_tables();
        for (auto const& table : tables) {
            total_num_events += table->get_num_messages();
        }
    } catch (std::exception const& e) {
        throw ClpFfiJsException{
                clp::ErrorCode::ErrorCode_Failure,
                __FILENAME__,
                __LINE__,
                std::format("Failed to read tables: {}", e.what())
        };
    }

    // Close and reopen so we can iterate fresh later
    archive_reader->close();
    archive_reader = std::make_shared<clp_s::ArchiveReader>();
    buffer_reader = std::make_shared<clp::BufferReader>(data_buffer.data(), length);
    archive_reader->open(buffer_reader, "sfa_archive");
    archive_reader->read_dictionaries_and_metadata();
    archive_reader->open_packed_streams();

    return std::unique_ptr<ArchiveDecoder>(
            new ArchiveDecoder(std::move(data_buffer), std::move(archive_reader), total_num_events)
    );
}

ArchiveDecoder::ArchiveDecoder(
        ystdlib::containers::Array<char> data_buffer,
        std::shared_ptr<clp_s::ArchiveReader> archive_reader,
        size_t total_num_events
)
        : m_data_buffer{std::move(data_buffer)},
          m_archive_reader{std::move(archive_reader)},
          m_total_num_events{total_num_events} {}

ArchiveDecoder::~ArchiveDecoder() {
    try {
        m_archive_reader->close();
    } catch (...) {
        // Suppress exceptions in destructor
    }
}

auto ArchiveDecoder::open_archive_from_buffer() -> std::shared_ptr<clp_s::ArchiveReader> {
    auto buffer_reader
            = std::make_shared<clp::BufferReader>(m_data_buffer.data(), m_data_buffer.size());
    auto archive_reader = std::make_shared<clp_s::ArchiveReader>();
    archive_reader->open(buffer_reader, "sfa_archive");
    archive_reader->read_dictionaries_and_metadata();
    archive_reader->open_packed_streams();
    return archive_reader;
}

auto ArchiveDecoder::get_file_names() const -> FileNamesTsType {
    auto names{emscripten::val::array()};
    auto const& range_index = m_archive_reader->get_range_index();
    for (auto const& entry : range_index) {
        auto it = entry.fields.find("_filename");
        if (entry.fields.end() != it && it->is_string()) {
            names.call<void>("push", emscripten::val(it->get<std::string>()));
        }
    }
    return FileNamesTsType{names};
}

auto ArchiveDecoder::get_metadata() const -> MetadataTsType {
    auto metadata{emscripten::val::object()};

    metadata.set("totalLogEventCount", emscripten::val(static_cast<double>(m_total_num_events)));

    // Get range index for file information
    auto const& range_index = m_archive_reader->get_range_index();
    auto files_array{emscripten::val::array()};
    for (auto const& entry : range_index) {
        auto file_obj{emscripten::val::object()};
        file_obj.set("startIndex", emscripten::val(static_cast<double>(entry.start_index)));
        file_obj.set("endIndex", emscripten::val(static_cast<double>(entry.end_index)));

        // Extract fields from the range index entry
        auto fields_obj{emscripten::val::object()};
        for (auto const& [key, value] : entry.fields.items()) {
            if (value.is_string()) {
                fields_obj.set(key, emscripten::val(value.get<std::string>()));
            } else if (value.is_number()) {
                fields_obj.set(key, emscripten::val(value.get<double>()));
            }
        }
        file_obj.set("fields", fields_obj);
        files_array.call<void>("push", file_obj);
    }
    metadata.set("files", files_array);

    return MetadataTsType{metadata};
}

auto ArchiveDecoder::decode_range(size_t begin_idx, size_t end_idx, bool use_filter)
        -> DecodedResultsTsType {
    if (begin_idx > end_idx) {
        return DecodedResultsTsType{emscripten::val::null()};
    }

    std::vector<LogEventData> const* events_ptr{nullptr};

    if (use_filter && nullptr != m_query_expr) {
        if (m_filter_dirty) {
            m_cached_filtered_events = read_filtered_events();
            m_filter_dirty = false;
        }
        events_ptr = &m_cached_filtered_events;
    } else {
        if (false == m_all_events_cached) {
            m_cached_all_events = read_all_events_unfiltered();
            m_all_events_cached = true;
        }
        events_ptr = &m_cached_all_events;
    }

    auto const& events = *events_ptr;
    if (end_idx > events.size()) {
        return DecodedResultsTsType{emscripten::val::null()};
    }

    auto results{emscripten::val::array()};
    for (size_t i = begin_idx; i < end_idx; ++i) {
        auto const& event = events[i];
        EM_ASM(
                {
                    Emval.toValue($0).push({
                        "logEventIdx" : $1,
                        "timestamp" : $2,
                        "message" : UTF8ToString($3),
                    });
                },
                results.as_handle(),
                static_cast<double>(event.log_event_idx),
                static_cast<double>(event.timestamp),
                event.message.c_str()
        );
    }

    return DecodedResultsTsType{results};
}

auto ArchiveDecoder::decode_range_by_file(
        std::string const& file_name,
        size_t begin_idx,
        size_t end_idx,
        bool use_filter
) -> DecodedResultsTsType {
    // Look up the file in the range index
    auto const& range_index = m_archive_reader->get_range_index();
    for (auto const& entry : range_index) {
        auto it = entry.fields.find("_filename");
        if (entry.fields.end() == it || false == it->is_string()) {
            continue;
        }
        if (it->get<std::string>() != file_name) {
            continue;
        }

        // Found the file — translate file-relative indices to global indices
        auto const file_event_count = entry.end_index - entry.start_index;
        if (begin_idx > end_idx || end_idx > file_event_count) {
            return DecodedResultsTsType{emscripten::val::null()};
        }

        auto const global_begin = entry.start_index + begin_idx;
        auto const global_end = entry.start_index + end_idx;
        return decode_range(global_begin, global_end, use_filter);
    }

    // File not found
    return DecodedResultsTsType{emscripten::val::null()};
}

void ArchiveDecoder::set_query(std::string const& kql_query) {
    std::istringstream kql_stream(kql_query);
    auto expr = clp_s::search::kql::parse_kql_expression(kql_stream);
    if (nullptr == expr) {
        throw ClpFfiJsException{
                clp::ErrorCode::ErrorCode_BadParam,
                __FILENAME__,
                __LINE__,
                std::format("Failed to parse KQL query: {}", kql_query)
        };
    }

    m_query_expr = std::move(expr);
    m_schema_match = std::make_shared<clp_s::search::SchemaMatch>(
            m_archive_reader->get_schema_tree(),
            m_archive_reader->get_schema_map()
    );
    m_filter_dirty = true;
}

void ArchiveDecoder::clear_query() {
    m_query_expr.reset();
    m_schema_match.reset();
    m_cached_filtered_events.clear();
    m_filter_dirty = true;
}

auto ArchiveDecoder::get_num_filtered_events() const -> size_t {
    if (nullptr == m_query_expr) {
        return m_total_num_events;
    }
    if (m_filter_dirty) {
        return 0;
    }
    return m_cached_filtered_events.size();
}

auto ArchiveDecoder::read_all_events_in_order() -> std::vector<LogEventData> {
    // Reopen archive for fresh iteration
    m_archive_reader->close();
    m_archive_reader = open_archive_from_buffer();

    auto tables = m_archive_reader->read_all_tables();

    std::vector<LogEventData> events;
    events.reserve(m_total_num_events);

    if (m_archive_reader->has_log_order()) {
        // Use priority queue for ordered iteration
        using ReaderPointer = std::shared_ptr<clp_s::SchemaReader>;
        auto cmp = [](ReaderPointer const& left, ReaderPointer const& right) {
            return left->get_next_log_event_idx() > right->get_next_log_event_idx();
        };
        std::priority_queue<ReaderPointer, std::vector<ReaderPointer>, decltype(cmp)> record_queue(
                cmp
        );

        for (auto& table : tables) {
            if (false == table->done()) {
                record_queue.push(std::move(table));
            }
        }

        while (false == record_queue.empty()) {
            auto next = record_queue.top();
            record_queue.pop();

            clp_s::epochtime_t timestamp = next->get_next_timestamp();
            int64_t log_event_idx = next->get_next_log_event_idx();
            std::string message;

            if (next->get_next_message(message)) {
                events.push_back(LogEventData{log_event_idx, timestamp, std::move(message)});
            }

            if (false == next->done()) {
                record_queue.push(std::move(next));
            }
        }
    } else {
        // No ordering — iterate tables sequentially
        for (auto& table : tables) {
            while (false == table->done()) {
                clp_s::epochtime_t timestamp = table->get_next_timestamp();
                int64_t log_event_idx = table->get_next_log_event_idx();
                std::string message;

                if (table->get_next_message(message)) {
                    events.push_back(LogEventData{log_event_idx, timestamp, std::move(message)});
                }
            }
        }
    }

    return events;
}

auto ArchiveDecoder::read_all_events_unfiltered() -> std::vector<LogEventData> {
    return read_all_events_in_order();
}

auto ArchiveDecoder::read_filtered_events() -> std::vector<LogEventData> {
    if (nullptr == m_query_expr) {
        return read_all_events_unfiltered();
    }

    // Reopen archive for fresh iteration
    m_archive_reader->close();
    m_archive_reader = open_archive_from_buffer();

    // Set up schema matching and run it to populate schema-to-query mapping
    auto schema_match = std::make_shared<clp_s::search::SchemaMatch>(
            m_archive_reader->get_schema_tree(),
            m_archive_reader->get_schema_map()
    );

    auto expr_copy = m_query_expr->copy();
    auto matched_expr = schema_match->run(expr_copy);
    if (std::dynamic_pointer_cast<clp_s::search::ast::EmptyExpr>(matched_expr)) {
        return {};
    }

    auto query_runner = std::make_shared<clp_s::search::QueryRunner>(
            schema_match,
            matched_expr,
            m_archive_reader,
            false  // ignore_case
    );
    query_runner->global_init();

    std::vector<LogEventData> events;
    auto const& schema_ids = m_archive_reader->get_schema_ids();

    for (auto const schema_id : schema_ids) {
        auto eval_result = query_runner->schema_init(schema_id);
        if (clp_s::EvaluatedValue::False == eval_result) {
            continue;
        }

        auto& schema_reader = m_archive_reader->read_schema_table(schema_id, true, true);
        schema_reader.initialize_filter(query_runner.get());

        while (false == schema_reader.done()) {
            std::string message;
            clp_s::epochtime_t timestamp{};
            int64_t log_event_idx{};

            if (schema_reader.get_next_message_with_metadata(
                        message,
                        timestamp,
                        log_event_idx,
                        query_runner.get()
                ))
            {
                events.push_back(LogEventData{log_event_idx, timestamp, std::move(message)});
            }
        }
    }

    // Sort by log event index for ordered output
    std::sort(events.begin(), events.end(), [](auto const& a, auto const& b) {
        return a.log_event_idx < b.log_event_idx;
    });

    return events;
}
}  // namespace clp_ffi_js::sfa
