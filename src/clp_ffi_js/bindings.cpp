#include <emscripten/bind.h>
#include "clp_ffi_js/ir/StreamReader.hpp"
#include "clp_ffi_js/ir/KVPairIRStreamReader.hpp"
#include "clp_ffi_js/ir/IRStreamReader.hpp"

namespace {
    EMSCRIPTEN_BINDINGS(ClpIrStreamReader) {
        emscripten::register_type<clp_ffi_js::ir::DataArrayTsType>("Uint8Array");
        emscripten::register_type<clp_ffi_js::ir::DecodedResultsTsType>(
                "Array<[string, number, number, number]>"
        );
        emscripten::register_type<clp_ffi_js::ir::FilteredLogEventMapTsType>("number[] | null");
        emscripten::register_type<clp_ffi_js::ir::ReaderOptions>("interface{logLevelKey?: string, timestampKey?: string}");

        emscripten::class_<clp_ffi_js::ir::IRStreamReader,
                emscripten::base<clp_ffi_js::ir::StreamReader>>("ClpIrStreamReader")
                .constructor(
                        &clp_ffi_js::ir::IRStreamReader::create,
                        emscripten::return_value_policy::take_ownership()
                )
                .function(
                        "getNumEventsBuffered",
                        &clp_ffi_js::ir::IRStreamReader::get_num_events_buffered
                )
                .function(
                        "getFilteredLogEventMap",
                        &clp_ffi_js::ir::IRStreamReader::get_filtered_log_event_map
                )
                .function("filterLogEvents", &clp_ffi_js::ir::IRStreamReader::filter_log_events)
                .function("deserializeStream", &clp_ffi_js::ir::IRStreamReader::deserialize_stream)
                .function("decodeRange", &clp_ffi_js::ir::IRStreamReader::decode_range);

        emscripten::class_<clp_ffi_js::ir::KVPairIRStreamReader,
                emscripten::base<clp_ffi_js::ir::StreamReader>>("ClpKVPairIRStreamReader")
                .constructor(
                        &clp_ffi_js::ir::KVPairIRStreamReader::create,
                        emscripten::return_value_policy::take_ownership()
                )
                .function(
                        "getNumEventsBuffered",
                        &clp_ffi_js::ir::KVPairIRStreamReader::get_num_events_buffered
                )
                .function(
                        "getFilteredLogEventMap",
                        &clp_ffi_js::ir::KVPairIRStreamReader::get_filtered_log_event_map
                )
                .function("filterLogEvents", &clp_ffi_js::ir::KVPairIRStreamReader::filter_log_events)
                .function(
                        "deserializeStream",
                        &clp_ffi_js::ir::KVPairIRStreamReader::deserialize_stream
                )
                .function("decodeRange", &clp_ffi_js::ir::KVPairIRStreamReader::decode_range);

        emscripten::class_<clp_ffi_js::ir::StreamReader>("ClpStreamReader")
                .constructor(
                        &clp_ffi_js::ir::StreamReader::create,
                        emscripten::return_value_policy::take_ownership()
                );
    }
}  // namespace