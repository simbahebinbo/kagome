// /**
//  * Copyright Quadrivium LLC
//  * All Rights Reserved
//  * SPDX-License-Identifier: Apache-2.0
//  */

// #pragma once

// #include "protobuf_state_response.hpp"
// #include "network/types/state_response_compressed.hpp"

// #include <zstd.h>
// #include <zstd_errors.h>

// static std::vector<uint8_t> decompressZstd(const std::vector<uint8_t>& compressed_data) {
//     // Determine the decompressed size
//     unsigned long long const decompressed_size = ZSTD_getFrameContentSize(compressed_data.data(), compressed_data.size());
//     if (decompressed_size == ZSTD_CONTENTSIZE_ERROR) {
//         throw std::runtime_error("Invalid compressed data.");
//     }
//     if (decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN) {
//         throw std::runtime_error("Unknown decompressed size.");
//     }

//     // Prepare a buffer to hold the decompressed data
//     std::vector<uint8_t> decompressed_data(decompressed_size);

//     // Decompress the data
//     size_t const result = ZSTD_decompress(
//         decompressed_data.data(),
//         decompressed_data.size(),
//         compressed_data.data(),
//         compressed_data.size()
//     );

//     if (ZSTD_isError(result)) {
//         throw std::runtime_error(ZSTD_getErrorName(result));
//     }

//     return decompressed_data;
// }

// namespace kagome::network {

//   template <>
//   struct ProtobufMessageAdapter<StateResponseCompressed> {
//     static size_t size(const StateResponseCompressed &t) {
//         return 0;
//     }

//     static std::vector<uint8_t>::iterator write(
//         const StateResponseCompressed &t,
//         std::vector<uint8_t> &out,
//         std::vector<uint8_t>::iterator loaded) {
//       ::api::v1::StateResponseCompressed msg;
//       msg.set_payload(t.data.data(), t.data.size());
//       return appendToVec(msg, out, loaded);
//     }

//     static outcome::result<std::vector<uint8_t>::const_iterator> read(
//         StateResponse &out,
//         const std::vector<uint8_t> &src,
//         std::vector<uint8_t>::const_iterator from) {
//       // const auto remains = src.size() - std::distance(src.begin(), from);
//       // assert(remains >= size(out));
//       auto state_response_compressed = decompressZstd(src);
//       ::api::v1::StateResponse msg;
//       if (!msg.ParseFromArray(state_response_compressed.data(), state_response_compressed.size())) {
//         return AdaptersError::PARSE_FAILED;
//       }
//       for (const auto &kvEntry : msg.entries()) {
//         KeyValueStateEntry kv;
//         auto root = kvEntry.state_root();
//         if (!root.empty()) {
//           kv.state_root = storage::trie::RootHash::fromString(root).value();
//         }
//         for (const auto &sEntry : kvEntry.entries()) {
//           kv.entries.emplace_back(
//               StateEntry{common::Buffer().put(sEntry.key()),
//                          common::Buffer().put(sEntry.value())});
//         }
//         kv.complete = kvEntry.complete();

//         out.entries.emplace_back(std::move(kv));
//       }
//       out.proof = common::Buffer().put(msg.proof());

//       std::advance(from, msg.ByteSizeLong());
//       return from;
//     }
//   };

// }  // namespace kagome::network
