#pragma once
#include <memory>
#include <exception>

#include <zstd.h>
#include <zstd_errors.h>

#include "zstd_stream_compressor.h"

std::vector<uint8_t> ZstdStreamCompressor::compress(std::span<uint8_t> data) {
    return compressZSTD(data);
}

std::vector<uint8_t> ZstdStreamCompressor::decompress(std::span<uint8_t> compressedData) {
    std::unique_ptr<ZSTD_DCtx, void(*)(ZSTD_DCtx*)> dctx(
        ZSTD_createDCtx(),
        [](ZSTD_DCtx* d) { ZSTD_freeDCtx(d); }
    );
    if (dctx == nullptr) {
        throw std::runtime_error("Failed to create ZSTD decompression context");
    }

    std::vector<uint8_t> decompressedData;
    size_t bufferSize = ZSTD_DStreamOutSize();
    std::vector<uint8_t> outBuffer(bufferSize);

    ZSTD_inBuffer input = { compressedData.data(), compressedData.size(), 0 };
    ZSTD_outBuffer output = { outBuffer.data(), outBuffer.size(), 0 };

    while (input.pos < input.size) {
        size_t ret = ZSTD_decompressStream(dctx.get(), &output, &input);
        if (ZSTD_isError(ret)) {
            throw std::runtime_error(ZSTD_getErrorName(ret));
        }

        decompressedData.insert(decompressedData.end(), outBuffer.data(), outBuffer.data() + output.pos);
        output.pos = 0;
    }

    return decompressedData;
}

std::vector<uint8_t> ZstdStreamCompressor::compressZSTD(std::span<uint8_t> inputData, int compressionLevel) {
    std::unique_ptr<ZSTD_CCtx, void(*)(ZSTD_CCtx*)> cctx(
        ZSTD_createCCtx(),
        [](ZSTD_CCtx* c) { ZSTD_freeCCtx(c); }
    );

    if (!cctx) {
        throw std::runtime_error("Failed to create ZSTD compression context");
    }

    size_t maxCompressedSize = ZSTD_compressBound(inputData.size());
    std::vector<uint8_t> compressedData(maxCompressedSize);

    ZSTD_inBuffer input = { inputData.data(), inputData.size(), 0 };
    ZSTD_outBuffer output = { compressedData.data(), compressedData.size(), 0 };

    while (input.pos < input.size) {
        size_t ret = ZSTD_compressStream(cctx.get(), &output, &input);
        if (ZSTD_isError(ret)) {
            throw std::runtime_error(ZSTD_getErrorName(ret));
        }
    }

    size_t remaining;
    do {
        remaining = ZSTD_endStream(cctx.get(), &output);
        if (ZSTD_isError(remaining)) {
            throw std::runtime_error(ZSTD_getErrorName(remaining));
        }
    } while (remaining > 0);

    compressedData.resize(output.pos);

    return compressedData;
}
