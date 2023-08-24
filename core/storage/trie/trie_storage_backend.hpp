/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KAGOME_TRIE_DB_BACKEND_HPP
#define KAGOME_TRIE_DB_BACKEND_HPP

#include <outcome/outcome.hpp>

#include "common/buffer.hpp"
#include "storage/buffer_map_types.hpp"
#include "storage/trie/polkadot_trie/trie_node.hpp"

namespace kagome::storage::trie {

  /**
   * Adapter for the trie node storage that allows to hide keyspace separation
   * along with root hash storing logic from the trie db component
   */
  class TrieNodeStorageBackend : public BufferStorage {
   public:
    ~TrieNodeStorageBackend() override = default;
  };

  /**
   * Adapter for the trie value storage that allows to hide keyspace separation
   * along with root hash storing logic from the trie db component
   */
  class TrieValueStorageBackend : public BufferStorage {
   public:
    ~TrieValueStorageBackend() override = default;
  };

}  // namespace kagome::storage::trie

#endif  // KAGOME_TRIE_DB_BACKEND_HPP
