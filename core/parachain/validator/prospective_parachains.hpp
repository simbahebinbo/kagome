/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KAGOME_PARACHAIN_PROSPECTIVE_PARACHAINS_HPP
#define KAGOME_PARACHAIN_PROSPECTIVE_PARACHAINS_HPP

#include <boost/variant.hpp>
#include <map>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "network/types/collator_messages.hpp"
#include "parachain/types.hpp"
#include "parachain/validator/collations.hpp"
#include "parachain/validator/fragment_tree.hpp"
#include "utils/map.hpp"

namespace kagome::parachain {

  class ProspectiveParachains {
    struct RelayBlockViewData {
      // Scheduling info for paras and upcoming paras.
      std::unordered_map<ParachainId, fragment::FragmentTree> fragment_trees;
      std::unordered_set<CandidateHash> pending_availability;
    };

    struct View {
      // Active or recent relay-chain blocks by block hash.
      std::unordered_map<Hash, RelayBlockViewData> active_leaves;
      std::unordered_map<ParachainId, fragment::CandidateStorage>
          candidate_storage;
    };

    View view;
    std::shared_ptr<crypto::Hasher> hasher_;
    log::Logger logger =
        log::createLogger("ProspectiveParachains", "parachain");

   public:
    ProspectiveParachains(std::shared_ptr<crypto::Hasher> hasher)
        : hasher_{std::move(hasher)} {}

    std::optional<runtime::PersistedValidationData>
    answerProspectiveValidationDataRequest(
        const RelayHash &candidate_relay_parent,
        const Hash &parent_head_data_hash,
        ParachainId para_id) {
      if (auto it = view.candidate_storage.find(para_id);
          it != view.candidate_storage.end()) {
        fragment::CandidateStorage &storage = it->second;
        std::optional<HeadData> head_data =
            utils::fromRefToOwn(storage.headDataByHash(parent_head_data_hash));
        std::optional<fragment::RelayChainBlockInfo> relay_parent_info{};
        std::optional<size_t> max_pov_size{};

        for (const auto &[_, x] : view.active_leaves) {
          auto it = x.fragment_trees.find(para_id);
          if (it == x.fragment_trees.end()) {
            continue;
          }
          const fragment::FragmentTree &fragment_tree = it->second;

          if (head_data && relay_parent_info && max_pov_size) {
            break;
          }
          if (!relay_parent_info) {
            relay_parent_info = utils::fromRefToOwn(
                fragment_tree.scope.ancestorByHash(candidate_relay_parent));
          }
          if (!head_data) {
            const auto &required_parent =
                fragment_tree.scope.base_constraints.required_parent;
            if (crypto::Hashed<const HeadData &, 32>{required_parent}.getHash()
                == parent_head_data_hash) {
              head_data = required_parent;
            }
          }
          if (!max_pov_size) {
            if (fragment_tree.scope.ancestorByHash(candidate_relay_parent)) {
              max_pov_size = fragment_tree.scope.base_constraints.max_pov_size;
            }
          }
        }

        if (head_data && relay_parent_info && max_pov_size) {
          return runtime::PersistedValidationData{
            .parent_head = *head_data,
            .relay_parent_number = relay_parent_info->number,
            .relay_parent_storage_root : relay_parent_info->storage_root,
            .max_pov_size = *max_pov_size
          };
        }
      }
      return std::nullopt;
    }

    /// @brief calculates hypothetical candidate and fragment tree membership
    /// @param candidates Candidates, in arbitrary order, which should be
    /// checked for possible membership in fragment trees.
    /// @param fragment_tree_relay_parent Either a specific fragment tree to
    /// check, otherwise all.
    /// @param backed_in_path_only Only return membership if all candidates in
    /// the path from the root are backed.
    std::vector<
        std::pair<HypotheticalCandidate, fragment::FragmentTreeMembership>>
    answerHypotheticalFrontierRequest(
        const gsl::span<const HypotheticalCandidate> &candidates,
        const std::optional<std::reference_wrapper<const Hash>>
            &fragment_tree_relay_parent,
        bool backed_in_path_only) {
      std::vector<
          std::pair<HypotheticalCandidate, fragment::FragmentTreeMembership>>
          response;
      response.reserve(candidates.size());
      std::transform(candidates.begin(),
                     candidates.end(),
                     std::back_inserter(response),
                     [](const HypotheticalCandidate &candidate)
                         -> std::pair<HypotheticalCandidate,
                                      fragment::FragmentTreeMembership> {
                       return {candidate, {}};
                     });

      const auto &required_active_leaf = fragment_tree_relay_parent;
      for (const auto &[active_leaf, leaf_view] : view.active_leaves) {
        if (required_active_leaf
            && required_active_leaf->get() != active_leaf) {
          continue;
        }

        for (auto &[c, membership] : response) {
          const ParachainId &para_id = candidatePara(c);
          auto it_fragment_tree = leaf_view.fragment_trees.find(para_id);
          if (it_fragment_tree == leaf_view.fragment_trees.end()) {
            continue;
          }

          auto it_candidate_storage = view.candidate_storage.find(para_id);
          if (it_candidate_storage == view.candidate_storage.end()) {
            continue;
          }

          const auto &fragment_tree = it_fragment_tree->second;
          const auto &candidate_storage = it_candidate_storage->second;
          const auto &candidate_hash = candidateHash(c);
          const auto &hypothetical = c;

          std::vector<size_t> depths =
              fragment_tree.hypotheticalDepths(candidate_hash,
                                               hypothetical,
                                               candidate_storage,
                                               backed_in_path_only);

          if (!depths.empty()) {
            membership.emplace_back(active_leaf, std::move(depths));
          }
        }
      }
      return response;
    }

    fragment::FragmentTreeMembership fragmentTreeMembership(
        const std::unordered_map<Hash, RelayBlockViewData> &active_leaves,
        ParachainId para,
        const CandidateHash &candidate) const {
      fragment::FragmentTreeMembership membership{};
      for (const auto &[relay_parent, view_data] : active_leaves) {
        if (auto it = view_data.fragment_trees.find(para);
            it != view_data.fragment_trees.end()) {
          const auto &tree = it->second;
          if (auto depths = tree.candidate(candidate)) {
            membership.emplace_back(relay_parent, *depths);
          }
        }
      }
      return membership;
    }

    void candidateSeconded(ParachainId para,
                           const CandidateHash &candidate_hash) {
      auto it = view.candidate_storage.find(para);
      if (it == view.candidate_storage.end()) {
        SL_WARN(logger,
                "Received instruction to second unknown candidate. (para "
                "id={}, candidate hash={})",
                para,
                candidate_hash);
        return;
      }

      auto &storage = it->second;
      if (!storage.contains(candidate_hash)) {
        SL_WARN(logger,
                "Received instruction to second unknown candidate in storage. "
                "(para "
                "id={}, candidate hash={})",
                para,
                candidate_hash);
        return;
      }

      storage.markSeconded(candidate_hash);
    }

    void candidateBacked(ParachainId para,
                         const CandidateHash &candidate_hash) {
      auto storage = view.candidate_storage.find(para);
      if (storage == view.candidate_storage.end()) {
        SL_WARN(logger,
                "Received instruction to back unknown candidate. (para_id={}, "
                "candidate_hash={})",
                para,
                candidate_hash);
        return;
      }
      if (!storage->second.contains(candidate_hash)) {
        SL_WARN(logger,
                "Received instruction to back unknown candidate. (para_id={}, "
                "candidate_hash={})",
                para,
                candidate_hash);
        return;
      }
      if (storage->second.isBacked(candidate_hash)) {
        SL_DEBUG(logger,
                 "Received redundant instruction to mark candidate as backed. "
                 "(para_id={}, candidate_hash={})",
                 para,
                 candidate_hash);
        return;
      }
      storage->second.markBacked(candidate_hash);
    }

    fragment::FragmentTreeMembership introduceCandidate(
        ParachainId para,
        const network::CommittedCandidateReceipt &candidate,
        const crypto::Hashed<runtime::PersistedValidationData, 32> &pvd,
        const CandidateHash &candidate_hash) {
      auto it_storage = view.candidate_storage.find(para);
      if (it_storage == view.candidate_storage.end()) {
        SL_WARN(logger,
                "Received seconded candidate for inactive para. (parachain "
                "id={}, candidate hash={})",
                para,
                candidate_hash);
        return {};
      }

      auto &storage = it_storage->second;
      if (auto res =
              storage.addCandidate(candidate_hash, candidate, pvd, hasher_);
          res.has_error()) {
        if (res.error()
            == fragment::CandidateStorage::Error::CANDIDATE_ALREADY_KNOWN) {
          return fragmentTreeMembership(
              view.active_leaves, para, candidate_hash);
        }
        if (res.error()
            == fragment::CandidateStorage::Error::
                PERSISTED_VALIDATION_DATA_MISMATCH) {
          SL_WARN(logger,
                  "Received seconded candidate had mismatching validation "
                  "data. (parachain id={}, candidate hash={})",
                  para,
                  candidate_hash);
          return {};
        }
      }

      fragment::FragmentTreeMembership membership{};
      for (auto &[relay_parent, leaf_data] : view.active_leaves) {
        if (auto it = leaf_data.fragment_trees.find(para);
            it != leaf_data.fragment_trees.end()) {
          auto &tree = it->second;
          tree.addAndPopulate(candidate_hash, storage);
          if (auto depths = tree.candidate(candidate_hash)) {
            membership.emplace_back(relay_parent, *depths);
          }
        }
      }

      if (membership.empty()) {
        storage.removeCandidate(candidate_hash, hasher_);
      }

      return membership;
    }
  };

}  // namespace kagome::parachain

#endif  // KAGOME_PARACHAIN_PROSPECTIVE_PARACHAINS_HPP
