/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <memory>
#include <exception>

#include <zstd.h>
#include <zstd_errors.h>

#include <qtils/enum_error_code.hpp>

#include "zstd_stream_compressor.h"

namespace kagome::network {

enum class ZstdStreamCompressorError {
    UNKOWN,
    COMPRESSION_ERROR,
    DECOMPRESSION_ERROR,
};

Q_ENUM_ERROR_CODE(ZstdStreamCompressorError) {
    using E = decltype(e);
    switch (e) {
        case E::COMPRESSION_ERROR:
            return "ZstdStreamCompressorError::COMPRESSION_ERROR";
        case E::DECOMPRESSION_ERROR:
            return "ZstdStreamCompressorError::DECOMPRESSION_ERROR";
        default:
            return "ZstdStreamCompressorError::UNKOWN";
    }
}

outcome::result<std::vector<uint8_t>> ZstdStreamCompressor::compress(std::span<uint8_t> data) try {
    std::unique_ptr<ZSTD_CCtx, void(*)(ZSTD_CCtx*)> cctx(
        ZSTD_createCCtx(),
        [](ZSTD_CCtx* c) { ZSTD_freeCCtx(c); }
    );

    if (!cctx) {
        // return CompressionError{ "Failed to create ZSTD compression context" };
        return ZstdStreamCompressorError::COMPRESSION_ERROR;
    }

    const auto setParameterResult = ZSTD_CCtx_setParameter(cctx.get(), ZSTD_c_compressionLevel, m_compressionLevel);
    if (ZSTD_isError(setParameterResult)) {
        // return CompressionError{ ZSTD_getErrorName(setParameterResult) };
        return ZstdStreamCompressorError::COMPRESSION_ERROR;
    }

    const auto maxCompressedSize = ZSTD_compressBound(data.size());
    std::vector<uint8_t> compressedData(maxCompressedSize);

    ZSTD_inBuffer input = { data.data(), data.size(), 0 };
    ZSTD_outBuffer output = { compressedData.data(), compressedData.size(), 0 };

    while (input.pos < input.size) {
        const auto compressResult = ZSTD_compressStream(cctx.get(), &output, &input);
        if (ZSTD_isError(compressResult)) {
            // return CompressionError{ ZSTD_getErrorName(compressResult) };
            return ZstdStreamCompressorError::COMPRESSION_ERROR;
        }
    }

    size_t remaining;
    do {
        remaining = ZSTD_endStream(cctx.get(), &output);
        if (ZSTD_isError(remaining)) {
            // return CompressionError{ ZSTD_getErrorName(remaining) };
            return ZstdStreamCompressorError::COMPRESSION_ERROR;
        }
    } while (remaining > 0);

    compressedData.resize(output.pos);

    return compressedData;
} catch (const std::exception& e) {
    // return CompressionError{ std::string{e.what()} };
    return ZstdStreamCompressorError::COMPRESSION_ERROR;
}
catch (...) {
    // return CompressionError{ "Unknown exception" };
    return ZstdStreamCompressorError::COMPRESSION_ERROR;
}

outcome::result<std::vector<uint8_t>> ZstdStreamCompressor::decompress(std::span<uint8_t> compressedData) try {
    std::unique_ptr<ZSTD_DCtx, void(*)(ZSTD_DCtx*)> dctx(
        ZSTD_createDCtx(),
        [](ZSTD_DCtx* d) { ZSTD_freeDCtx(d); }
    );
    if (dctx == nullptr) {
        return ZstdStreamCompressorError::DECOMPRESSION_ERROR;
        // return CompressionError{ "Failed to create ZSTD decompression context" };
    }

    std::vector<uint8_t> decompressedData;
    std::vector<uint8_t> outBuffer(ZSTD_DStreamOutSize());

    ZSTD_inBuffer input = { compressedData.data(), compressedData.size(), 0 };
    ZSTD_outBuffer output = { outBuffer.data(), outBuffer.size(), 0 };

    while (input.pos < input.size) {
        size_t ret = ZSTD_decompressStream(dctx.get(), &output, &input);
        if (ZSTD_isError(ret)) {
            // return CompressionError{ ZSTD_getErrorName(ret) };
            return ZstdStreamCompressorError::DECOMPRESSION_ERROR;
        }

        decompressedData.insert(decompressedData.end(), outBuffer.data(), outBuffer.data() + output.pos);
        output.pos = 0;
    }

    return decompressedData;
} catch (const std::exception& e) {
    // return CompressionError{ std::string{e.what()} };
    return ZstdStreamCompressorError::DECOMPRESSION_ERROR;
} catch (...) {
    // return CompressionError{ "Unknown exception" };
    return ZstdStreamCompressorError::DECOMPRESSION_ERROR;
}

}  // namespace kagome::network
