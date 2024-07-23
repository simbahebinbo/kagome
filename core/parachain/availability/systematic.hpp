/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <deque>
#include <ec-cpp/ec-cpp.hpp>

#include "parachain/availability/erasure_coding_error.hpp"
#include "runtime/runtime_api/parachain_host_types.hpp"

namespace kagome::parachain {

/// `RecoveryStrategy` that attempts to recover the systematic chunks from the validators that
/// hold them, in order to bypass the erasure code reconstruction step, which is costly.
struct FetchSystematicChunks {
    using ErasureChunkContext = std::pair<std::optional<ErasureChunk>, network::ProtocolName>;
    using ErasureChunkResult = outcome::result<ErasureChunkContext>;
    using OngoingRequests = std::deque<std::tuple<libp2p::peer::PeerId, ValidatorIndex, ErasureChunkResult>>;

    /// Parameters needed for fetching systematic chunks.
    struct FetchSystematicChunksParams {
        /// Validators that hold the systematic chunks.
        std::vector<std::pair<ChunkIndex, ValidatorIndex>> validators;

        /// Validators in the backing group, to be used as a backup for requesting systematic chunks.
        std::vector<ValidatorIndex> backers;
    };

	/// Systematic recovery threshold.
	size_t threshold;

	/// Validators that hold the systematic chunks.
	std::vector<std::pair<ChunkIndex, ValidatorIndex>> validators;

	/// Backers to be used as a backup.
	std::vector<ValidatorIndex> backers;

	/// Collection of in-flight requests.
	requesting_chunks: OngoingRequests,

    static FetchSystematicChunks create(FetchSystematicChunksParams &&params);
    bool is_unavailable(size_t unrequested_validators, size_t in_flight_requests, size_t systematic_chunk_count, size_t threshold) const;

};


}  // namespace kagome::parachain
