#pragma once

#include <memory>
#include <exception>

#include <zstd.h>
#include <zstd_errors.h>

#include "compressor.h"

struct ZstdStreamCompressor : public ICompressor {
    ZstdStreamCompressor(int compressionLevel = 3) : m_compressionLevel(compressionLevel) {}
    std::vector<uint8_t> compress(std::span<uint8_t> data) override;
    std::vector<uint8_t> decompress(std::span<uint8_t> compressedData) override;
private:
    int m_compressionLevel;
};
