// Copyright (c) 2018-2026 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_LLMQ_QUORUMCACHE_H
#define BITCOIN_LLMQ_QUORUMCACHE_H

#include <llmq/params.h>
#include <llmq/types.h>
#include <saltedhasher.h>
#include <sync.h>
#include <unordered_lru_cache.h>

#include <map>
#include <vector>

class CBlockIndex;
namespace Consensus {
struct Params;
} // namespace Consensus

namespace llmq {

/**
 * Chain-derived quorum resolution caches. Contents depend on which
 * mined-commitment records answer a lookup, so a chainstate must not share
 * them with another chainstate.
 */
class QuorumResolutionCache
{
public:
    explicit QuorumResolutionCache(const Consensus::Params& consensus_params);

    mutable Mutex m_cs_maps;
    mutable std::map<Consensus::LLMQType, Uint256LruHashMap<CQuorumPtr>> mapQuorumsCache
        GUARDED_BY(m_cs_maps);
    mutable std::map<Consensus::LLMQType, Uint256LruHashMap<std::vector<CQuorumCPtr>>> scanQuorumsCache
        GUARDED_BY(m_cs_maps);

    // On mainnet, we have around 62 quorums active at any point; let's cache a little more than double that to be safe.
    // it maps `quorum_hash` to `pindex`
    mutable Mutex cs_quorumBaseBlockIndexCache;
    mutable Uint256LruHashMap<const CBlockIndex*, /*max_size=*/128> quorumBaseBlockIndexCache
        GUARDED_BY(cs_quorumBaseBlockIndexCache);
};

} // namespace llmq

#endif // BITCOIN_LLMQ_QUORUMCACHE_H
