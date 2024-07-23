/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "parachain/availability/systematic.hpp"

#include <fmt/format.h>

namespace kagome::parachain {
    FetchSystematicChunks FetchSystematicChunks::create(FetchSystematicChunks::FetchSystematicChunksParams &&params) {
        const auto threshold = params.validators.size();
		return FetchSystematicChunks {
			.threshold = threshold,
			.validators = std::move(params.validators),
			.backers = std::move(params.backers),
			.requesting_chunks = {},
		};
    }

    bool FetchSystematicChunks::is_unavailable(size_t received_chunks, size_t requesting_chunks, size_t unrequested_validators, size_t threshold) const {
        return received_chunks + requesting_chunks + unrequested_validators < threshold;
    }
}
