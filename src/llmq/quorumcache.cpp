// Copyright (c) 2018-2026 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <llmq/quorumcache.h>

#include <llmq/utils.h>

namespace llmq {

QuorumResolutionCache::QuorumResolutionCache(const Consensus::Params& consensus_params)
{
    utils::InitQuorumsCache(mapQuorumsCache, consensus_params, /*limit_by_connections=*/false);
}

} // namespace llmq
