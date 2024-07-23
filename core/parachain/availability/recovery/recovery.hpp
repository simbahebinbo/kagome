/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "crypto/type_hasher.hpp"
#include "runtime/runtime_api/parachain_host_types.hpp"

namespace kagome::parachain {

  /// Used to recover PoV and validation data from remote validators inside
  /// validator group. This operation is important in Approval step to validate
  /// availability and correctness of the candidate.
  class Recovery {
   public:
    /// We try the backing group first if PoV size is lower than specified, then fallback to
    /// validator chunks.
    using BackersFirstIfSizeLower = Tagged<size_t, struct BackersFirstIfSizeLowerTag>;

      /// We try the backing group first if PoV size is lower than specified, then fallback to
      /// systematic chunks. Regular chunk recovery as a last resort.
      using BackersFirstIfSizeLowerThenSystematicChunks = Tagged<size_t, struct BackersFirstIfSizeLowerThenSystematicChunksTag>;

      /// The following variants are only helpful for integration tests.
      ///
      /// We always try the backing group first, then fallback to validator chunks.
      using BackersFirstAlways = Tagged<Empty, struct BackersFirstAlwaysTag>;

      /// We always recover using validator chunks.
      using ChunksAlways = Tagged<Empty, struct ChunksAlwaysTag>;

      /// First try the backing group. Then systematic chunks.
      using BackersThenSystematicChunks = Tagged<Empty, struct BackersThenSystematicChunksTag>;

      /// Always recover using systematic chunks, fall back to regular chunks.
      using SystematicChunks = Tagged<Empty, struct SystematicChunksTag>;

    /// The strategy we use to recover the PoV.
    using RecoveryStrategyKind  = boost::variant<
      BackersFirstIfSizeLower
      BackersFirstIfSizeLowerThenSystematicChunks,
      BackersFirstAlways,
      ChunksAlways,
      BackersThenSystematicChunks,
      SystematicChunks
    >;

    using AvailableData = runtime::AvailableData;
    using Cb =
        std::function<void(std::optional<outcome::result<AvailableData>>)>;
    using HashedCandidateReceipt = crypto::
        Hashed<network::CandidateReceipt, 32, crypto::Blake2b_StreamHasher<32>>;

    virtual ~Recovery() = default;

    virtual void remove(const CandidateHash &candidate) = 0;
    virtual void recover(const HashedCandidateReceipt &hashed_receipt,
                         SessionIndex session_index,
                         std::optional<GroupIndex> backing_group,
                         std::optional<CoreIndex> maybe_core_index,
                         const RecoveryStrategyKind &recovery_strategy_kind,
                         Cb cb) = 0;
  };
}  // namespace kagome::parachain
