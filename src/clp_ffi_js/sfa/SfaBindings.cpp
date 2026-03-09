#include <emscripten/bind.h>

#include <clp_ffi_js/sfa/ArchiveDecoder.hpp>

namespace {
EMSCRIPTEN_BINDINGS(ClpSfaReader) {
    // JS types used as inputs
    emscripten::register_type<clp_ffi_js::sfa::DataArrayTsType>("Uint8Array");

    // JS types used as outputs
    emscripten::register_type<clp_ffi_js::sfa::FileNamesTsType>("Array<string>");
    emscripten::register_type<clp_ffi_js::sfa::MetadataTsType>(
            "{totalLogEventCount: number,"
            " files: Array<{startIndex: number, endIndex: number,"
            " fields: Record<string, string | number>}>}"
    );
    emscripten::register_type<clp_ffi_js::sfa::DecodedResultsTsType>(
            "Array<{logEventIdx: number, timestamp: number, message: string}> | null"
    );

    emscripten::class_<clp_ffi_js::sfa::ArchiveDecoder>("ClpSfaReader")
            .constructor(
                    &clp_ffi_js::sfa::ArchiveDecoder::create,
                    emscripten::return_value_policy::take_ownership()
            )
            .function("getFileNames", &clp_ffi_js::sfa::ArchiveDecoder::get_file_names)
            .function("getMetadata", &clp_ffi_js::sfa::ArchiveDecoder::get_metadata)
            .function("getNumEvents", &clp_ffi_js::sfa::ArchiveDecoder::get_num_events)
            .function("decodeRange", &clp_ffi_js::sfa::ArchiveDecoder::decode_range)
            .function(
                    "decodeRangeByFile",
                    &clp_ffi_js::sfa::ArchiveDecoder::decode_range_by_file
            )
            .function("setQuery", &clp_ffi_js::sfa::ArchiveDecoder::set_query)
            .function("clearQuery", &clp_ffi_js::sfa::ArchiveDecoder::clear_query)
            .function(
                    "getNumFilteredEvents",
                    &clp_ffi_js::sfa::ArchiveDecoder::get_num_filtered_events
            );
}
}  // namespace
