/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KAGOME_PARACHAIN_FRAGMENT_TREE
#define KAGOME_PARACHAIN_FRAGMENT_TREE

#include <boost/variant.hpp>
#include <map>
#include <optional>
#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "outcome/outcome.hpp"

#include "crypto/hasher/hasher_impl.hpp"
#include "log/logger.hpp"
#include "network/types/collator_messages.hpp"
#include "network/types/collator_messages_vstaging.hpp"
#include "parachain/types.hpp"
#include "parachain/validator/collations.hpp"
#include "primitives/common.hpp"
#include "primitives/math.hpp"
#include "runtime/runtime_api/parachain_host_types.hpp"
#include "utils/map.hpp"

namespace kagome::parachain::fragment {

  template <typename... Args>
  using HashMap = std::unordered_map<Args...>;
  template <typename... Args>
  using HashSet = std::unordered_set<Args...>;
  template <typename... Args>
  using Vec = std::vector<Args...>;
  using BitVec = scale::BitVec;
  using ParaId = ParachainId;
  template <typename... Args>
  using Option = std::optional<Args...>;
  template <typename... Args>
  using Map = std::map<Args...>;
  using FragmentTreeMembership = Vec<std::pair<Hash, Vec<size_t>>>;

  struct ProspectiveCandidate {
    /// The commitments to the output of the execution.
    network::CandidateCommitments commitments;
    /// The collator that created the candidate.
    CollatorId collator;
    /// The signature of the collator on the payload.
    runtime::CollatorSignature collator_signature;
    /// The persisted validation data used to create the candidate.
    runtime::PersistedValidationData persisted_validation_data;
    /// The hash of the PoV.
    Hash pov_hash;
    /// The validation code hash used by the candidate.
    ValidationCodeHash validation_code_hash;
  };

  /// The state of a candidate.
  ///
  /// Candidates aren't even considered until they've at least been seconded.
  enum CandidateState {
    /// The candidate has been introduced in a spam-protected way but
    /// is not necessarily backed.
    Introduced,
    /// The candidate has been seconded.
    Seconded,
    /// The candidate has been completely backed by the group.
    Backed,
  };

  struct CandidateEntry {
    CandidateHash candidate_hash;
    RelayHash relay_parent;
    ProspectiveCandidate candidate;
    CandidateState state;
  };

  struct CandidateStorage {
    enum class Error {
      CANDIDATE_ALREADY_KNOWN,
      PERSISTED_VALIDATION_DATA_MISMATCH,
    };

    // Index from head data hash to candidate hashes with that head data as a
    // parent.
    HashMap<Hash, HashSet<CandidateHash>> by_parent_head;

    // Index from head data hash to candidate hashes outputting that head data.
    HashMap<Hash, HashSet<CandidateHash>> by_output_head;

    // Index from candidate hash to fragment node.
    HashMap<CandidateHash, CandidateEntry> by_candidate_hash;

    outcome::result<void> addCandidate(
        const CandidateHash &candidate_hash,
        const network::CommittedCandidateReceipt &candidate,
        const crypto::Hashed<const runtime::PersistedValidationData &,
                             32,
                             crypto::Blake2b_StreamHasher<32>>
            &persisted_validation_data,
        const std::shared_ptr<crypto::Hasher> &hasher);

    Option<std::reference_wrapper<const CandidateEntry>> get(
        const CandidateHash &candidate_hash) const {
      if (auto it = by_candidate_hash.find(candidate_hash);
          it != by_candidate_hash.end()) {
        return {{it->second}};
      }
      return std::nullopt;
    }

    Option<Hash> relayParentByCandidateHash(
        const CandidateHash &candidate_hash) const {
      if (auto it = by_candidate_hash.find(candidate_hash);
          it != by_candidate_hash.end()) {
        return it->second.relay_parent;
      }
      return std::nullopt;
    }

    bool contains(const CandidateHash &candidate_hash) const {
      return by_candidate_hash.find(candidate_hash) != by_candidate_hash.end();
    }

    template <typename F>
    void iterParaChildren(const Hash &parent_head_hash, F &&func) const {
      if (auto it = by_parent_head.find(parent_head_hash);
          it != by_parent_head.end()) {
        for (const auto &h : it->second) {
          if (auto c_it = by_candidate_hash.find(h);
              c_it != by_candidate_hash.end()) {
            std::forward<F>(func)(c_it->second);
          }
        }
      }
    }

    Option<std::reference_wrapper<const HeadData>> headDataByHash(
        const Hash &hash) const {
      auto search = [&](const auto &container)
          -> Option<std::reference_wrapper<const CandidateEntry>> {
        if (auto it = container.find(hash); it != container.end()) {
          if (!it->second.empty()) {
            const CandidateHash &a_candidate = *it->second.begin();
            return get(a_candidate);
          }
        }
        return std::nullopt;
      };

      if (auto e = search(by_output_head)) {
        return {{e->get().candidate.commitments.para_head}};
      }
      if (auto e = search(by_parent_head)) {
        return {{e->get().candidate.persisted_validation_data.parent_head}};
      }
      return std::nullopt;
    }

    void removeCandidate(const CandidateHash &candidate_hash,
                         const std::shared_ptr<crypto::Hasher> &hasher) {
      if (auto it = by_candidate_hash.find(candidate_hash);
          it != by_candidate_hash.end()) {
        const auto parent_head_hash = hasher->blake2b_256(
            it->second.candidate.persisted_validation_data.parent_head);
        if (auto it_bph = by_parent_head.find(parent_head_hash);
            it_bph != by_parent_head.end()) {
          it_bph->second.erase(candidate_hash);
          if (it_bph->second.empty()) {
            by_parent_head.erase(it_bph);
          }
        }
        by_candidate_hash.erase(it);
      }
    }

    void markSeconded(const CandidateHash &candidate_hash) {
      if (auto it = by_candidate_hash.find(candidate_hash);
          it != by_candidate_hash.end()) {
        if (it->second.state != CandidateState::Backed) {
          it->second.state = CandidateState::Seconded;
        }
      }
    }

    void markBacked(const CandidateHash &candidate_hash) {
      if (auto it = by_candidate_hash.find(candidate_hash);
          it != by_candidate_hash.end()) {
        it->second.state = CandidateState::Backed;
      }
    }

    bool isBacked(const CandidateHash &candidate_hash) const {
      return by_candidate_hash.count(candidate_hash) > 0
          && by_candidate_hash.at(candidate_hash).state
                 == CandidateState::Backed;
    }

    std::pair<size_t, size_t> len() const {
      return std::make_pair(by_parent_head.size(), by_candidate_hash.size());
    }
  };

  using NodePointerRoot = network::Empty;
  using NodePointerStorage = size_t;
  using NodePointer = boost::variant<NodePointerRoot, NodePointerStorage>;

  struct RelayChainBlockInfo {
    /// The hash of the relay-chain block.
    Hash hash;
    /// The number of the relay-chain block.
    BlockNumber number;
    /// The storage-root of the relay-chain block.
    Hash storage_root;
  };

  inline bool validate_against_constraints(
      const Constraints &constraints,
      const RelayChainBlockInfo &relay_parent,
      const ProspectiveCandidate &candidate,
      const ConstraintModifications &modifications) {
    runtime::PersistedValidationData expected_pvd{
        .parent_head = constraints.required_parent,
        .relay_parent_number = relay_parent.number,
        .relay_parent_storage_root = relay_parent.storage_root,
        .max_pov_size = uint32_t(constraints.max_pov_size),
    };

    if (expected_pvd != candidate.persisted_validation_data) {
      return false;
    }

    if (constraints.validation_code_hash != candidate.validation_code_hash) {
      return false;
    }

    if (relay_parent.number < constraints.min_relay_parent_number) {
      return false;
    }

    size_t announced_code_size = 0ull;
    if (candidate.commitments.opt_para_runtime) {
      if (constraints.upgrade_restriction
          && *constraints.upgrade_restriction == UpgradeRestriction::Present) {
        return false;
      }
      announced_code_size = candidate.commitments.opt_para_runtime->size();
    }

    if (announced_code_size > constraints.max_code_size) {
      return false;
    }

    if (modifications.dmp_messages_processed == 0) {
      if (!constraints.dmp_remaining_messages.empty()
          && constraints.dmp_remaining_messages[0] <= relay_parent.number) {
        return false;
      }
    }

    if (candidate.commitments.outbound_hor_msgs.size()
        > constraints.max_hrmp_num_per_candidate) {
      return false;
    }

    if (candidate.commitments.upward_msgs.size()
        > constraints.max_ump_num_per_candidate) {
      return false;
    }

    /// TODO(iceseer): do
    /// add error-codes for each case

    return constraints.checkModifications(modifications);
  }

  struct Fragment {
    /// The new relay-parent.
    RelayChainBlockInfo relay_parent;
    /// The constraints this fragment is operating under.
    Constraints operating_constraints;
    /// The core information about the prospective candidate.
    ProspectiveCandidate candidate;
    /// Modifications to the constraints based on the outputs of
    /// the candidate.
    ConstraintModifications modifications;

    const RelayChainBlockInfo &relayParent() const {
      return relay_parent;
    }

    static Option<Fragment> create(const RelayChainBlockInfo &relay_parent,
                                   const Constraints &operating_constraints,
                                   const ProspectiveCandidate &candidate) {
      const network::CandidateCommitments &commitments = candidate.commitments;

      std::unordered_map<ParachainId, OutboundHrmpChannelModification>
          outbound_hrmp;
      std::optional<ParachainId> last_recipient;
      for (size_t i = 0; i < commitments.outbound_hor_msgs.size(); ++i) {
        const network::OutboundHorizontal &message =
            commitments.outbound_hor_msgs[i];
        if (last_recipient) {
          if (*last_recipient >= message.para_id) {
            return std::nullopt;
          }
        }
        last_recipient = message.para_id;
        OutboundHrmpChannelModification &record =
            outbound_hrmp[message.para_id];

        record.bytes_submitted += message.upward_msg.size();
        record.messages_submitted += 1;
      }

      size_t ump_sent_bytes = 0ull;
      for (const auto &m : commitments.upward_msgs) {
        ump_sent_bytes += m.size();
      }

      ConstraintModifications modifications{
          .required_parent = commitments.para_head,
          .hrmp_watermark = ((commitments.watermark == relay_parent.number)
                                 ? HrmpWatermarkUpdate{HrmpWatermarkUpdateHead{
                                     .v = commitments.watermark}}
                                 : HrmpWatermarkUpdate{HrmpWatermarkUpdateTrunk{
                                     .v = commitments.watermark}}),
          .outbound_hrmp = outbound_hrmp,
          .ump_messages_sent = commitments.upward_msgs.size(),
          .ump_bytes_sent = ump_sent_bytes,
          .dmp_messages_processed = commitments.downward_msgs_count,
          .code_upgrade_applied =
              operating_constraints.future_validation_code
                  ? (relay_parent.number
                     >= operating_constraints.future_validation_code->first)
                  : false,
      };

      if (!validate_against_constraints(
              operating_constraints, relay_parent, candidate, modifications)) {
        return std::nullopt;
      }

      return Fragment{
          .relay_parent = relay_parent,
          .operating_constraints = operating_constraints,
          .candidate = candidate,
          .modifications = modifications,
      };
    }

    const ConstraintModifications &constraintModifications() const {
      return modifications;
    }
  };

  struct FragmentNode {
    // A pointer to the parent node.
    NodePointer parent;
    Fragment fragment;
    CandidateHash candidate_hash;
    size_t depth;
    ConstraintModifications cumulative_modifications;
    Vec<std::pair<NodePointer, CandidateHash>> children;

    const Hash &relayParent() const {
      return fragment.relayParent().hash;
    }

    Option<NodePointer> candidateChild(
        const CandidateHash &candidate_hash) const {
      auto it =
          std::find_if(children.begin(),
                       children.end(),
                       [&](const std::pair<NodePointer, CandidateHash> &p) {
                         return p.second == candidate_hash;
                       });
      if (it != children.end()) {
        return it->first;
      }
      return std::nullopt;
    }
  };

  struct PendingAvailability {
    /// The candidate hash.
    CandidateHash candidate_hash;
    /// The block info of the relay parent.
    RelayChainBlockInfo relay_parent;
  };

  struct Scope {
    enum class Error {
      UNEXPECTED_ANCESTOR,
    };

    ParaId para;
    RelayChainBlockInfo relay_parent;
    Map<BlockNumber, RelayChainBlockInfo> ancestors;
    HashMap<Hash, RelayChainBlockInfo> ancestors_by_hash;
    Vec<PendingAvailability> pending_availability;
    Constraints base_constraints;
    size_t max_depth;

    static outcome::result<Scope> withAncestors(
        ParachainId para,
        const RelayChainBlockInfo &relay_parent,
        const Constraints &base_constraints,
        const Vec<PendingAvailability> &pending_availability,
        size_t max_depth,
        const Vec<RelayChainBlockInfo> &ancestors);

    const RelayChainBlockInfo &earliestRelayParent() const {
      if (!ancestors.empty()) {
        return ancestors.begin()->second;
      }
      return relay_parent;
    }

    Option<std::reference_wrapper<const PendingAvailability>>
    getPendingAvailability(const CandidateHash &candidate_hash) const {
      auto it = std::find_if(pending_availability.begin(),
                             pending_availability.end(),
                             [&](const PendingAvailability &c) {
                               return c.candidate_hash == candidate_hash;
                             });
      if (it != pending_availability.end()) {
        return {{*it}};
      }
      return std::nullopt;
    }

    Option<std::reference_wrapper<const RelayChainBlockInfo>> ancestorByHash(
        const Hash &hash) const {
      if (hash == relay_parent.hash) {
        return {{relay_parent}};
      }
      if (auto it = ancestors_by_hash.find(hash);
          it != ancestors_by_hash.end()) {
        return {{it->second}};
      }
      return std::nullopt;
    }
  };

  /// This is a tree of candidates based on some underlying storage of
  /// candidates and a scope.
  ///
  /// All nodes in the tree must be either pending availability or within the
  /// scope. Within the scope means it's built off of the relay-parent or an
  /// ancestor.
  struct FragmentTree {
    Scope scope;

    // Invariant: a contiguous prefix of the 'nodes' storage will contain
    // the top-level children.
    Vec<FragmentNode> nodes;

    // The candidates stored in this tree, mapped to a bitvec indicating the
    // depths where the candidate is stored.
    HashMap<CandidateHash, BitVec> candidates;

    std::shared_ptr<crypto::Hasher> hasher_;
    log::Logger logger = log::createLogger("parachain", "fragment_tree");

    Option<Vec<size_t>> candidate(const CandidateHash &hash) const {
      if (auto it = candidates.find(hash); it != candidates.end()) {
        Vec<size_t> res;
        for (size_t ix = 0; ix < it->second.bits.size(); ++ix) {
          if (it->second.bits[ix]) {
            res.emplace_back(ix);
          }
        }
        return res;
      }
      return std::nullopt;
    }

    std::vector<CandidateHash> getCandidates() const {
      auto kv = std::views::keys(candidates);
      return {kv.begin(), kv.end()};
    }

    template <typename Func>
    std::optional<CandidateHash> selectChild(
        const std::vector<CandidateHash> &required_path, Func &&pred) const {
      NodePointer base_node{NodePointerRoot{}};
      for (const CandidateHash &required_step : required_path) {
        if (auto node = nodeCandidateChild(base_node, required_step)) {
          base_node = *node;
        } else {
          return std::nullopt;
        }
      }

      return visit_in_place(
          base_node,
          [&](const NodePointerRoot &) -> std::optional<CandidateHash> {
            for (const FragmentNode &n : nodes) {
              if (!is_type<NodePointerRoot>(n.parent)) {
                return std::nullopt;
              }
              if (scope.getPendingAvailability(n.candidate_hash)) {
                return std::nullopt;
              }
              if (!pred(n.candidate_hash)) {
                return std::nullopt;
              }
              return n.candidate_hash;
            }
            return std::nullopt;
          },
          [&](const NodePointerStorage &ptr) -> std::optional<CandidateHash> {
            for (const auto &[_, h] : nodes[ptr].children) {
              if (scope.getPendingAvailability(h)) {
                return std::nullopt;
              }
              if (!pred(h)) {
                return std::nullopt;
              }
              return h;
            }
            return std::nullopt;
          });
    }

    static FragmentTree populate(const std::shared_ptr<crypto::Hasher> &hasher,
                                 const Scope &scope,
                                 const CandidateStorage &storage) {
      auto logger = log::createLogger("parachain", "fragment_tree");
      SL_TRACE(logger,
               "Instantiating Fragment Tree. (relay parent={}, relay parent "
               "num={}, para id={}, ancestors={})",
               scope.relay_parent.hash,
               scope.relay_parent.number,
               scope.para,
               scope.ancestors.size());

      FragmentTree tree{
          .scope = scope, .nodes = {}, .candidates = {}, .hasher_ = hasher};

      tree.populateFromBases(storage, {{NodePointerRoot{}}});
      return tree;
    }

    void populateFromBases(const CandidateStorage &storage,
                           const std::vector<NodePointer> &initial_bases) {
      Option<size_t> last_sweep_start{};
      do {
        const auto sweep_start = nodes.size();
        if (last_sweep_start && *last_sweep_start == sweep_start) {
          break;
        }

        Vec<NodePointer> parents;
        if (last_sweep_start) {
          parents.reserve(nodes.size() - *last_sweep_start);
          for (size_t ix = *last_sweep_start; ix < nodes.size(); ++ix) {
            parents.emplace_back(ix);
          }
        } else {
          parents = initial_bases;
        }

        for (const auto &parent_pointer : parents) {
          const auto &[modifications, child_depth, earliest_rp] =
              visit_in_place(
                  parent_pointer,
                  [&](const NodePointerRoot &)
                      -> std::tuple<ConstraintModifications,
                                    size_t,
                                    RelayChainBlockInfo> {
                    return std::make_tuple(ConstraintModifications{},
                                           size_t{0ull},
                                           scope.earliestRelayParent());
                  },
                  [&](const NodePointerStorage &ptr)
                      -> std::tuple<ConstraintModifications,
                                    size_t,
                                    RelayChainBlockInfo> {
                    const auto &node = nodes[ptr];
                    if (auto opt_rcbi =
                            scope.ancestorByHash(node.relayParent())) {
                      return std::make_tuple(node.cumulative_modifications,
                                             size_t(node.depth + 1),
                                             opt_rcbi->get());
                    } else {
                      if (auto c = scope.getPendingAvailability(
                              node.candidate_hash)) {
                        return std::make_tuple(node.cumulative_modifications,
                                               size_t(node.depth + 1),
                                               c->get().relay_parent);
                      }
                      UNREACHABLE;
                    }
                  });

          if (child_depth > scope.max_depth) {
            continue;
          }

          auto child_constraints_res =
              scope.base_constraints.applyModifications(modifications);
          if (child_constraints_res.has_error()) {
            SL_TRACE(logger,
                     "Failed to apply modifications. (error={})",
                     child_constraints_res.error().message());
            continue;
          }

          const auto &child_constraints = child_constraints_res.value();
          const auto required_head_hash =
              hasher_->blake2b_256(child_constraints.required_parent);

          storage.iterParaChildren(
              required_head_hash, [&](const CandidateEntry &candidate) {
                auto pending =
                    scope.getPendingAvailability(candidate.candidate_hash);
                Option<RelayChainBlockInfo> relay_parent_opt;
                if (pending) {
                  relay_parent_opt = pending->get().relay_parent;
                } else {
                  relay_parent_opt = utils::fromRefToOwn(
                      scope.ancestorByHash(candidate.relay_parent));
                }
                if (!relay_parent_opt) {
                  return;
                }
                auto &relay_parent = *relay_parent_opt;

                uint32_t min_relay_parent_number;
                if (pending) {
                  min_relay_parent_number = visit_in_place(
                      parent_pointer,
                      [&](const NodePointerStorage &) {
                        return earliest_rp.number;
                      },
                      [&](const NodePointerRoot &) {
                        return pending->get().relay_parent.number;
                      });
                } else {
                  min_relay_parent_number = std::max(
                      earliest_rp.number, scope.earliestRelayParent().number);
                }

                if (relay_parent.number < min_relay_parent_number) {
                  return;
                }

                if (nodeHasCandidateChild(parent_pointer,
                                          candidate.candidate_hash)) {
                  return;
                }

                auto constraints = child_constraints;
                if (pending) {
                  constraints.min_relay_parent_number =
                      pending->get().relay_parent.number;
                }

                Option<Fragment> f = Fragment::create(
                    relay_parent, constraints, candidate.candidate);
                if (!f) {
                  SL_TRACE(logger,
                           "Failed to instantiate fragment. (relay parent={}, "
                           "candidate hash={})",
                           relay_parent.hash,
                           candidate.candidate_hash);
                  return;
                }

                Fragment &fragment = *f;
                ConstraintModifications cumulative_modifications =
                    modifications;
                cumulative_modifications.stack(
                    fragment.constraintModifications());

                insertNode(FragmentNode{
                    .parent = parent_pointer,
                    .fragment = fragment,
                    .candidate_hash = candidate.candidate_hash,
                    .depth = child_depth,
                    .cumulative_modifications = cumulative_modifications,
                    .children = {}});
              });
        }

        last_sweep_start = sweep_start;
      } while (true);
    }

    void addAndPopulate(const CandidateHash &hash,
                        const CandidateStorage &storage) {
      auto opt_candidate_entry = storage.get(hash);
      if (!opt_candidate_entry) {
        return;
      }

      const auto &candidate_entry = opt_candidate_entry->get();
      const auto &candidate_parent =
          candidate_entry.candidate.persisted_validation_data.parent_head;

      Vec<NodePointer> bases{};
      if (scope.base_constraints.required_parent == candidate_parent) {
        bases.emplace_back(NodePointerRoot{});
      }

      for (size_t ix = 0ull; ix < nodes.size(); ++ix) {
        const auto &n = nodes[ix];
        if (n.cumulative_modifications.required_parent
            && n.cumulative_modifications.required_parent.value()
                   == candidate_parent) {
          bases.emplace_back(ix);
        }
      }

      populateFromBases(storage, bases);
    }

    void insertNode(FragmentNode &&node) {
      const NodePointerStorage pointer{nodes.size()};
      const auto parent_pointer = node.parent;
      const auto &candidate_hash = node.candidate_hash;
      const auto max_depth = scope.max_depth;

      auto &bv = candidates[candidate_hash];
      if (bv.bits.size() == 0ull) {
        bv.bits.resize(max_depth + 1);
      }
      bv.bits[node.depth] = true;

      visit_in_place(
          parent_pointer,
          [&](const NodePointerRoot &) {
            if (nodes.empty()
                || is_type<NodePointerRoot>(nodes.back().parent)) {
              nodes.emplace_back(std::move(node));
            } else {
              nodes.insert(
                  std::find_if(nodes.begin(),
                               nodes.end(),
                               [](const auto &item) {
                                 return !is_type<NodePointerRoot>(item.parent);
                               }),
                  std::move(node));
            }
          },
          [&](const NodePointerStorage &ptr) {
            nodes.emplace_back(std::move(node));
            nodes[ptr].children.emplace_back(pointer, candidate_hash);
          });
    }

    Option<NodePointer> nodeCandidateChild(
        const NodePointer &pointer, const CandidateHash &candidate_hash) const {
      return visit_in_place(
          pointer,
          [&](const NodePointerStorage &ptr) -> Option<NodePointer> {
            if (ptr < nodes.size()) {
              return nodes[ptr].candidateChild(candidate_hash);
            }
            return std::nullopt;
          },
          [&](const NodePointerRoot &) -> Option<NodePointer> {
            for (size_t ix = 0ull; ix < nodes.size(); ++ix) {
              const FragmentNode &n = nodes[ix];
              if (!is_type<NodePointerRoot>(n.parent)) {
                break;
              }
              if (n.candidate_hash != candidate_hash) {
                continue;
              }
              return ix;
            }
            return std::nullopt;
          });
    }

    bool nodeHasCandidateChild(const NodePointer &pointer,
                               const CandidateHash &candidate_hash) const {
      return nodeCandidateChild(pointer, candidate_hash).has_value();
    }

    bool pathContainsBackedOnlyCandidates(
        NodePointer parent_pointer,
        const CandidateStorage &candidate_storage) const {
      while (auto ptr = if_type<NodePointerStorage>(parent_pointer)) {
        const auto &node = nodes[ptr->get()];
        const auto &candidate_hash = node.candidate_hash;

        auto opt_entry = candidate_storage.get(candidate_hash);
        if (!opt_entry || opt_entry->get().state != CandidateState::Backed) {
          return false;
        }
        parent_pointer = node.parent;
      }
      return true;
    }

    Vec<size_t> hypotheticalDepths(const CandidateHash &hash,
                                   const HypotheticalCandidate &candidate,
                                   const CandidateStorage &candidate_storage,
                                   bool backed_in_path_only) const {
      if (!backed_in_path_only) {
        if (auto it = candidates.find(hash); it != candidates.end()) {
          Vec<size_t> res;
          for (size_t ix = 0; ix < it->second.bits.size(); ++ix) {
            if (it->second.bits[ix]) {
              res.emplace_back(ix);
            }
          }
          return res;
        }
      }

      const auto crp = relayParent(candidate);
      auto candidate_relay_parent =
          [&]() -> Option<std::reference_wrapper<const RelayChainBlockInfo>> {
        if (scope.relay_parent.hash == crp.get()) {
          return {{scope.relay_parent}};
        }
        if (auto it = scope.ancestors_by_hash.find(crp);
            it != scope.ancestors_by_hash.end()) {
          return {{it->second}};
        }
        return std::nullopt;
      }();

      if (!candidate_relay_parent) {
        return {};
      }

      const auto max_depth = scope.max_depth;
      BitVec depths;
      depths.bits.resize(max_depth + 1);

      auto process_parent_pointer = [&](const NodePointer &parent_pointer) {
        const auto &[modifications, child_depth, earliest_rp] = visit_in_place(
            parent_pointer,
            [&](const NodePointerRoot &)
                -> std::tuple<
                    ConstraintModifications,
                    size_t,
                    std::reference_wrapper<const RelayChainBlockInfo>> {
              return std::make_tuple(ConstraintModifications{},
                                     size_t{0ull},
                                     scope.earliestRelayParent());
            },
            [&](const NodePointerStorage &ptr)
                -> std::tuple<
                    ConstraintModifications,
                    size_t,
                    std::reference_wrapper<const RelayChainBlockInfo>> {
              const auto &node = nodes[ptr];
              if (auto opt_rcbi = scope.ancestorByHash(node.relayParent())) {
                return std::make_tuple(node.cumulative_modifications,
                                       size_t(node.depth + 1),
                                       opt_rcbi->get());
              } else {
                if (auto r =
                        scope.getPendingAvailability(node.candidate_hash)) {
                  return std::make_tuple(node.cumulative_modifications,
                                         size_t(node.depth + 1),
                                         scope.earliestRelayParent());
                }
                UNREACHABLE;
              }
            });

        if (child_depth > max_depth) {
          return;
        }

        if (earliest_rp.get().number > candidate_relay_parent->get().number) {
          return;
        }

        auto child_constraints_res =
            scope.base_constraints.applyModifications(modifications);
        if (child_constraints_res.has_error()) {
          SL_TRACE(logger,
                   "Failed to apply modifications. (error={})",
                   child_constraints_res.error());
          return;
        }

        const auto &child_constraints = child_constraints_res.value();
        const auto parent_head_hash = parentHeadDataHash(*hasher_, candidate);

        /// TODO(iceseer): keep hashed object in constraints to avoid recalc
        if (parent_head_hash
            != hasher_->blake2b_256(child_constraints.required_parent)) {
          return;
        }

        if (auto const value =
                if_type<const HypotheticalCandidateComplete>(candidate)) {
          ProspectiveCandidate prospective_candidate{
              .commitments = value->get().receipt.commitments,
              .collator = value->get().receipt.descriptor.collator_id,
              .collator_signature = value->get().receipt.descriptor.signature,
              .persisted_validation_data =
                  value->get().persisted_validation_data,
              .pov_hash = value->get().receipt.descriptor.pov_hash,
              .validation_code_hash =
                  value->get().receipt.descriptor.validation_code_hash,
          };

          if (!Fragment::create(candidate_relay_parent->get(),
                                child_constraints,
                                prospective_candidate)) {
            return;
          }
        }

        if (!backed_in_path_only
            || pathContainsBackedOnlyCandidates(parent_pointer,
                                                candidate_storage)) {
          depths.bits[child_depth] = true;
        }
      };

      process_parent_pointer(NodePointerRoot{});
      for (size_t ix = 0; ix < nodes.size(); ++ix) {
        process_parent_pointer(ix);
      }

      Vec<size_t> res;
      for (size_t ix = 0; ix < depths.bits.size(); ++ix) {
        if (depths.bits[ix]) {
          res.emplace_back(ix);
        }
      }
      return res;
    }
  };

}  // namespace kagome::parachain::fragment

OUTCOME_HPP_DECLARE_ERROR(kagome::parachain::fragment, Constraints::Error);
OUTCOME_HPP_DECLARE_ERROR(kagome::parachain::fragment, Scope::Error);
OUTCOME_HPP_DECLARE_ERROR(kagome::parachain::fragment, CandidateStorage::Error);

#endif  // KAGOME_PARACHAIN_FRAGMENT_TREE
