/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

// #include "outcome/custom.hpp"
#include <qtils/enum_error_code.hpp>



namespace kagome::network {

enum class ZstdCompressionError {

  struct CompressionError {
    [[nodiscard]] const std::string &message() const {
      return msg;
    }
    std::string msg;
  };

  inline auto format_as(const CompressionError &e) {
    return e.msg;
  }

  inline void outcome_throw_as_system_error_with_payload(CompressionError e) {
    throw e;
  }

  using CompressionOutcome = CustomOutcome<std::vector<uint8_t>, CompressionError>;
}  // namespace kagome::network
