/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>
#include <exception>

#include <zstd.h>
#include <zstd_errors.h>

#include "compressor.h"

namespace kagome::network {
struct ZstdStreamCompressor : public ICompressor {
    ZstdStreamCompressor(int compressionLevel = 3) : m_compressionLevel(compressionLevel) {}
    CompressionOutcome compress(std::span<uint8_t> data) override;
    CompressionOutcome decompress(std::span<uint8_t> compressedData) override;
private:
    int m_compressionLevel;
};

}  // namespace kagome::network