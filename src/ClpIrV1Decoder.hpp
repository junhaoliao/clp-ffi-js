#ifndef CLP_FFI_JS_CLPIRV1DECODER_HPP
#define CLP_FFI_JS_CLPIRV1DECODER_HPP

#include <emscripten/val.h>

#include <clp/ffi/ir_stream/decoding_methods.hpp>
#include <clp/ir/LogEventDeserializer.hpp>
#include <clp/streaming_compression/zstd/Decompressor.hpp>

class ClpIrV1Decoder {
public:
    static auto create(emscripten::val const& data_array) -> ClpIrV1Decoder*;

    ClpIrV1Decoder(char const* data_buffer, size_t length);

    /**
     * Calculates the estimated number of events stored in the log.
     *
     * If `build_idx()` has not been called before, the function will return cFullRangeEndIdx=0,
     * indicating that there are no events stored in the log.
     *
     * @return The estimated number of events in the log.
     */
    size_t get_estimated_num_events();

    emscripten::val build_idx(size_t begin_idx, size_t end_idx);
    emscripten::val decode(size_t begin_idx, size_t end_idx);

private:
    std::unique_ptr<clp::ir::LogEventDeserializer<clp::ir::four_byte_encoded_variable_t>>
            m_deserializer;
    std::vector<clp::ir::LogEvent<clp::ir::four_byte_encoded_variable_t>> m_log_events;
    clp::TimestampPattern m_ts_pattern;
    clp::streaming_compression::zstd::Decompressor m_zstd_decompressor;

    /**
     * Creates a de-serializer by decoding the preamble of the IR stream, which includes the
     * encoding type. It also retrieves the timestamp pattern used in the stream.
     *
     * @return the created de-serializer.
     * @throw if any error occurs.
     */
    auto decode_preamble(char const* data_buffer, size_t length)
            -> clp::ir::LogEventDeserializer<clp::ir::four_byte_encoded_variable_t>;
};

#endif  // CLP_FFI_JS_CLPIRV1DECODER_HPP
