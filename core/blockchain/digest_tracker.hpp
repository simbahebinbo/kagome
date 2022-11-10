/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KAGOME_BLOCKCHAIN_DIGESTSTRACKER
#define KAGOME_BLOCKCHAIN_DIGESTSTRACKER

#include "outcome/outcome.hpp"
#include "primitives/common.hpp"
#include "primitives/digest.hpp"

namespace kagome::blockchain {

  class DigestTracker {
   public:
    virtual ~DigestTracker() = default;

    /**
     * Processes block digest
     * @param message
     * @return failure or nothing
     */
    virtual outcome::result<void> onDigest(
        const primitives::BlockInfo &block,
        const primitives::Digest &digest) = 0;

    /**
     * Cancels digest of applied block.
     * Should be called when the block is rolling back
     * @param block - corresponding block
     */
    virtual void cancel(const primitives::BlockInfo &block) = 0;
  };

}  // namespace kagome::blockchain

#endif  // KAGOME_BLOCKCHAIN_DIGESTSTRACKER
