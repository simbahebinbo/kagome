#pragma once

#include <cstdint>
#include <span>
#include <vector>

struct ICompressor {
  virtual ~ICompressor() = default;
  virtual std::vector<uint8_t> compress(std::span<uint8_t> data) = 0;
  virtual std::vector<uint8_t> decompress(std::span<uint8_t> compressedData) = 0;
};
