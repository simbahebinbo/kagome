#pragma once
#include <memory>
#include <exception>

#include <zstd.h>
#include <zstd_errors.h>

#include "compressor.h"

struct ZstdStreamCompressor : public ICompressor {
    std::vector<uint8_t> compress(std::span<uint8_t> data) override;
    std::vector<uint8_t> decompress(std::span<uint8_t> compressedData) override;
private:
    std::vector<uint8_t> compressZSTD(std::span<uint8_t> inputData, int compressionLevel = 3);
};
