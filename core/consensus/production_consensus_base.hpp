/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "consensus/production_consensus.hpp"

#include "clock/clock.hpp"
#include "consensus/sassafras/phase.hpp"
#include "consensus/sassafras/types/sassafras_configuration.hpp"
#include "injector/lazy.hpp"
#include "log/logger.hpp"
#include "metrics/metrics.hpp"
#include "primitives/block.hpp"
#include "primitives/event_types.hpp"
#include "telemetry/service.hpp"

namespace boost::asio {
  class io_context;
}

namespace kagome {
  class PoolHandlerReady;
}

namespace kagome::common {
  class MainThreadPool;
  class WorkerThreadPool;
}  // namespace kagome::common

namespace kagome::application {
  class AppConfiguration;
  class AppStateManager;
}  // namespace kagome::application

namespace kagome::authorship {
  class Proposer;
}

namespace kagome::blockchain {
  class BlockTree;
}

namespace kagome::common {
  class WorkerPoolHandler;
  class MainPoolHandler;
}  // namespace kagome::common

namespace kagome::consensus {
  class SlotsUtil;
}

namespace kagome::consensus::sassafras {
  class SassafrasConfigRepository;
  class SassafrasLottery;
  class SlotLeadership;
}  // namespace kagome::consensus::sassafras

namespace kagome::crypto {
  class SessionKeys;
  class Hasher;
}  // namespace kagome::crypto

namespace kagome::dispute {
  class DisputeCoordinator;
}

namespace kagome::parachain {
  class BitfieldStore;
  class BackingStore;
}  // namespace kagome::parachain

namespace kagome::network {
  class BlockAnnounceTransmitter;
}

namespace kagome::runtime {
  class OffchainWorkerApi;
}

namespace kagome::storage::changes_trie {
  class StorageChangesTrackerImpl;
}

namespace kagome::consensus {

  class ProductionConsensusBase
      : public ProductionConsensus,
        public std::enable_shared_from_this<ProductionConsensusBase> {
   public:
    struct Context {
      primitives::BlockInfo parent;
      EpochNumber epoch;
      SlotNumber slot;
      TimePoint slot_timestamp;
      std::shared_ptr<crypto::BandersnatchKeypair> keypair;
    };

    ProductionConsensusBase(
        application::AppStateManager &app_state_manager,
        const application::AppConfiguration &app_config,
        const clock::SystemClock &clock,
        std::shared_ptr<blockchain::BlockTree> block_tree,
        LazySPtr<SlotsUtil> slots_util,
        std::shared_ptr<sassafras::SassafrasConfigRepository>
            sassafras_config_repo,
        const EpochTimings &timings,
        std::shared_ptr<crypto::SessionKeys> session_keys,
        std::shared_ptr<sassafras::SassafrasLottery> lottery,
        std::shared_ptr<crypto::Hasher> hasher,
        std::shared_ptr<parachain::BitfieldStore> bitfield_store,
        std::shared_ptr<parachain::BackingStore> backing_store,
        std::shared_ptr<dispute::DisputeCoordinator> dispute_coordinator,
        std::shared_ptr<authorship::Proposer> proposer,
        primitives::events::StorageSubscriptionEnginePtr storage_sub_engine,
        primitives::events::ChainSubscriptionEnginePtr chain_sub_engine,
        std::shared_ptr<network::BlockAnnounceTransmitter> announce_transmitter,
        std::shared_ptr<runtime::OffchainWorkerApi> offchain_worker_api,
        common::MainThreadPool &main_thread_pool,
        common::WorkerThreadPool &worker_thread_pool);

    bool isGenesisConsensus() const override;

    ValidatorStatus getValidatorStatus(const primitives::BlockInfo &parent_info,
                                       EpochNumber epoch_number) const override;

    outcome::result<SlotNumber> getSlot(
        const primitives::BlockHeader &header) const override;

    outcome::result<void> processSlot(
        SlotNumber slot, const primitives::BlockInfo &best_block) override;

   private:
    outcome::result<void> processSlotLeadership(
        const Context &ctx, const sassafras::SlotLeadership &slot_leadership);

    outcome::result<primitives::PreRuntime> calculatePreDigest(
        const Context &ctx,
        const sassafras::SlotLeadership &slot_leadership) const;

    outcome::result<primitives::Seal> sealBlock(
        const Context &ctx, const primitives::Block &block) const;

    outcome::result<void> processSlotLeadershipProposed(
        const Context &ctx,
        uint64_t now,
        clock::SteadyClock::TimePoint proposal_start,
        std::shared_ptr<storage::changes_trie::StorageChangesTrackerImpl>
            &&changes_tracker,
        primitives::Block &&block);

    log::Logger log_;

    const clock::SystemClock &clock_;
    std::shared_ptr<blockchain::BlockTree> block_tree_;
    LazySPtr<SlotsUtil> slots_util_;
    std::shared_ptr<sassafras::SassafrasConfigRepository>
        sassafras_config_repo_;
    const EpochTimings &timings_;
    std::shared_ptr<crypto::SessionKeys> session_keys_;
    std::shared_ptr<sassafras::SassafrasLottery> lottery_;
    std::shared_ptr<crypto::Hasher> hasher_;
    std::shared_ptr<parachain::BitfieldStore> bitfield_store_;
    std::shared_ptr<parachain::BackingStore> backing_store_;
    std::shared_ptr<dispute::DisputeCoordinator> dispute_coordinator_;
    std::shared_ptr<authorship::Proposer> proposer_;
    primitives::events::StorageSubscriptionEnginePtr storage_sub_engine_;
    primitives::events::ChainSubscriptionEnginePtr chain_sub_engine_;
    std::shared_ptr<network::BlockAnnounceTransmitter> announce_transmitter_;
    std::shared_ptr<runtime::OffchainWorkerApi> offchain_worker_api_;
    std::shared_ptr<PoolHandlerReady> main_pool_handler_;
    std::shared_ptr<PoolHandlerReady> worker_pool_handler_;

    const bool is_validator_by_config_;
    bool is_active_validator_;

    // Metrics
    metrics::RegistryPtr metrics_registry_ = metrics::createRegistry();
    metrics::Gauge *metric_is_relaychain_validator_;

    telemetry::Telemetry telemetry_;  // telemetry
  };

}  // namespace kagome::consensus
