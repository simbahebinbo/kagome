/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "network/adapters/protobuf.hpp"

#include "network/protobuf/api.v1.pb.h"
#include "network/types/state_response.hpp"
#include "scale/scale.hpp"

#include <zstd.h>
#include <zstd_errors.h>

std::vector<uint8_t> decompressZstd(const std::vector<uint8_t>& compressedData) {
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

namespace kagome::network {

  template <>
  struct ProtobufMessageAdapter<StateResponse> {
    static size_t size(const StateResponse &t) {
      return 0;
    }

    static std::vector<uint8_t>::iterator write(
        const StateResponse &t,
        std::vector<uint8_t> &out,
        std::vector<uint8_t>::iterator loaded) {
      ::api::v1::StateResponse msg;
      for (const auto &entries : t.entries) {
        auto *dst_entries = msg.add_entries();
        if (entries.state_root) {
          dst_entries->set_state_root(entries.state_root->toString());
        }
        for (const auto &entry : entries.entries) {
          auto *dst_entry = dst_entries->add_entries();
          dst_entry->set_key(entry.key.toString());
          dst_entry->set_value(entry.value.toString());
        }
        dst_entries->set_complete(entries.complete);
      }
      msg.set_proof(t.proof.toString());

      return appendToVec(msg, out, loaded);
    }

    static outcome::result<std::vector<uint8_t>::const_iterator> read(
        StateResponse &out,
        const std::vector<uint8_t> &src,
        std::vector<uint8_t>::const_iterator from) {
      // const auto remains = src.size() - std::distance(src.begin(), from);
      // assert(remains >= size(out));

      auto state_response_decompressed = decompressZstd(src);

      ::api::v1::StateResponse msg;
      if (!msg.ParseFromArray(state_response_decompressed.data(), state_response_decompressed.size())) {
        return AdaptersError::PARSE_FAILED;
      }
      // if (!msg.ParseFromArray(from.base(), remains)) {
      //   return AdaptersError::PARSE_FAILED;
      // }

      for (const auto &kvEntry : msg.entries()) {
        KeyValueStateEntry kv;
        auto root = kvEntry.state_root();
        if (!root.empty()) {
          kv.state_root = storage::trie::RootHash::fromString(root).value();
        }
        for (const auto &sEntry : kvEntry.entries()) {
          kv.entries.emplace_back(
              StateEntry{common::Buffer().put(sEntry.key()),
                         common::Buffer().put(sEntry.value())});
        }
        kv.complete = kvEntry.complete();

        out.entries.emplace_back(std::move(kv));
      }
      out.proof = common::Buffer().put(msg.proof());

      std::advance(from, msg.ByteSizeLong());
      return from;
    }
  };

}  // namespace kagome::network
