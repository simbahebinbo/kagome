/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "error.h"
namespace kagome::network {
struct ICompressor {
  virtual ~ICompressor() = default;
  virtual CompressionOutcome compress(std::span<uint8_t> data) = 0;
  virtual CompressionOutcome decompress(std::span<uint8_t> compressedData) = 0;
};

}  // namespace kagome::network
