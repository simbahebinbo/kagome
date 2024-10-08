/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "runtime/runtime_api/block_builder.hpp"

namespace kagome::runtime {

  class Executor;

  class BlockBuilderImpl final : public BlockBuilder {
   public:
    explicit BlockBuilderImpl(std::shared_ptr<Executor> executor);

    outcome::result<primitives::ApplyExtrinsicResult> apply_extrinsic(
        RuntimeContext &ctx, const primitives::Extrinsic &extrinsic) override;

    outcome::result<primitives::BlockHeader> finalize_block(
        RuntimeContext &ctx) override;

    outcome::result<std::vector<primitives::Extrinsic>> inherent_extrinsics(
        RuntimeContext &ctx, const primitives::InherentData &data) override;

    outcome::result<primitives::CheckInherentsResult> check_inherents(
        const primitives::Block &block,
        const primitives::InherentData &data) override;

    outcome::result<common::Hash256> random_seed(
        const primitives::BlockHash &block) override;

   private:
    std::shared_ptr<Executor> executor_;
  };
}  // namespace kagome::runtime
