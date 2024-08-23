/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "protobuf_state_response.hpp"
#include "network/types/state_response_compressed.hpp"

namespace kagome::network {

  template <>
  struct ProtobufMessageAdapter<StateResponseCompressed> {
    static size_t size(const StateResponseCompressed &t) {
        return 0;
    }

    static std::vector<uint8_t>::iterator write(
        const StateResponseCompressed &t,
        std::vector<uint8_t> &out,
        std::vector<uint8_t>::iterator loaded) {
      ::api::v1::StateResponseCompressed msg;
      msg.set_payload(t.data.data(), t.data.size());
      return appendToVec(msg, out, loaded);
    }

    static outcome::result<std::vector<uint8_t>::const_iterator> read(
        StateResponseCompressed &out,
        const std::vector<uint8_t> &src,
        std::vector<uint8_t>::const_iterator from) {
      const auto remains = src.size() - std::distance(src.begin(), from);
      assert(remains >= size(out));

      ::api::v1::StateResponseCompressed msg;
      if (!msg.ParseFromArray(from.base(), remains)) {
        return AdaptersError::PARSE_FAILED;
      }
      out.data = common::Buffer().put(msg.payload());

      std::advance(from, msg.ByteSizeLong());
      return from;
    }
  };

}  // namespace kagome::network
