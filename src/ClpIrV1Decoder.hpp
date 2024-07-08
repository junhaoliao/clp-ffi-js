#ifndef CLP_FFI_JS_CLPIRV1DECODER_HPP
#define CLP_FFI_JS_CLPIRV1DECODER_HPP

#include <emscripten/val.h>

#include <clp/ffi/ir_stream/decoding_methods.hpp>
#include <clp/ir/LogEventDeserializer.hpp>
#include <clp/streaming_compression/zstd/Decompressor.hpp>

class ClpIrV1Decoder {
public:
    /**
     * Factory method to create a ClpIrV1Decoder instance by
     * - Copying an input array into C++ space.
     * - Constructing a Zstd Decompressor instance.
     * - Constructing a LogEventDeserializer by decoding the preamble of the IR stream, which
     * includes the encoding type.
     * - Passing those to the ClpIrV1Decoder() for their life cycles management.
     *
     * @param data_array JS Uint8Array which contains the Zstd-compressed CLP IR V1 bytes.
     * @return the created instance.
     * @throw std::exception if any error occurs.
     */
    [[nodiscard]] static auto create(emscripten::val const& data_array) -> ClpIrV1Decoder*;

    // Destructor
    ~ClpIrV1Decoder();

    // Explicitly disable copy and move constructor/assignment
    ClpIrV1Decoder(ClpIrV1Decoder const&) = delete;
    ClpIrV1Decoder& operator=(ClpIrV1Decoder const&) = delete;

    /**
     * Calculates the estimated number of events stored in the log.
     *
     * If `build_idx()` has not been called before, the function will return cFullRangeEndIdx=0,
     * indicating that there are no events stored in the log.
     *
     * @return The estimated number of events in the log.
     */
    [[nodiscard]] size_t get_estimated_num_events();

    [[nodiscard]] emscripten::val build_idx(size_t begin_idx, size_t end_idx);
    [[nodiscard]] emscripten::val decode(size_t begin_idx, size_t end_idx);

private:
    // Constructor
    explicit ClpIrV1Decoder(
            std::vector<char const> data_buffer,
            std::shared_ptr<clp::streaming_compression::zstd::Decompressor> zstd_decompressor,
            clp::ir::LogEventDeserializer<clp::ir::four_byte_encoded_variable_t> deserializer
    );

    std::vector<char const> m_data_buffer;
    clp::ir::LogEventDeserializer<clp::ir::four_byte_encoded_variable_t> m_deserializer;
    std::vector<clp::ir::LogEvent<clp::ir::four_byte_encoded_variable_t>> m_log_events;
    clp::TimestampPattern m_ts_pattern;
    std::shared_ptr<clp::streaming_compression::zstd::Decompressor> m_zstd_decompressor;
};

#endif  // CLP_FFI_JS_CLPIRV1DECODER_HPP
