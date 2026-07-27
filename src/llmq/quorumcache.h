// Copyright (c) 2018-2026 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_LLMQ_QUORUMCACHE_H
#define BITCOIN_LLMQ_QUORUMCACHE_H

#include <llmq/params.h>
#include <llmq/types.h>
#include <saltedhasher.h>
#include <sync.h>
#include <uint256.h>
#include <unordered_lru_cache.h>

#include <gsl/pointers.h>

#include <map>
#include <optional>
#include <utility>
#include <vector>

class CBlockIndex;
class CChain;
class CEvoDB;
class ChainstateManager;
namespace Consensus {
struct Params;
} // namespace Consensus

extern RecursiveMutex cs_main; // NOLINT(readability-redundant-declaration)

namespace llmq {
class CFinalCommitment;


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

/** Chain-derived mined-commitment records in EvoDB and their lookup cache. */
class MinedCommitmentsStore
{
private:
    CEvoDB& m_evoDb;
    const ChainstateManager& m_chainman;

    mutable Mutex m_cache_cs;
    // Cache the block in which a commitment was mined. Membership in a
    // particular chain is checked on every call so reorgs need no cache flush.
    mutable std::map<Consensus::LLMQType, Uint256LruHashMap<uint256>> mapMinedCommitmentBlockCache GUARDED_BY(m_cache_cs);

public:
    explicit MinedCommitmentsStore(CEvoDB& evoDb, const ChainstateManager& chainman);

    bool HasMinedCommitment(Consensus::LLMQType llmqType, const uint256& quorumHash) const
        EXCLUSIVE_LOCKS_REQUIRED(!m_cache_cs);
    bool HasMinedCommitment(Consensus::LLMQType llmqType, const uint256& quorumHash, const CChain& chain) const
        EXCLUSIVE_LOCKS_REQUIRED(::cs_main, !m_cache_cs);
    std::pair<CFinalCommitment, uint256> GetMinedCommitment(Consensus::LLMQType llmqType, const uint256& quorumHash) const;

    std::vector<const CBlockIndex*> GetMinedCommitmentsUntilBlock(Consensus::LLMQType llmqType, gsl::not_null<const CBlockIndex*> pindex, size_t maxCount) const;
    std::map<Consensus::LLMQType, std::vector<const CBlockIndex*>> GetMinedAndActiveCommitmentsUntilBlock(gsl::not_null<const CBlockIndex*> pindex) const;

    std::vector<const CBlockIndex*> GetMinedCommitmentsIndexedUntilBlock(Consensus::LLMQType llmqType, const CBlockIndex* pindex, size_t maxCount) const;
    std::vector<const CBlockIndex*> GetLastMinedCommitmentsPerQuorumIndexUntilBlock(Consensus::LLMQType llmqType,
                                                                                    const CBlockIndex* pindex,
                                                                                    size_t cycle) const;
    std::optional<const CBlockIndex*> GetLastMinedCommitmentsByQuorumIndexUntilBlock(Consensus::LLMQType llmqType, const CBlockIndex* pindex, int quorumIndex, size_t cycle) const;

    void WriteMinedCommitment(const CFinalCommitment& qc, const uint256& block_hash, int mined_height,
                              int quorum_base_height, bool rotation_enabled) EXCLUSIVE_LOCKS_REQUIRED(!m_cache_cs);
    void EraseMinedCommitment(const CFinalCommitment& qc, int mined_height, bool rotation_enabled)
        EXCLUSIVE_LOCKS_REQUIRED(!m_cache_cs);
    void WriteBestBlockUpgrade(const uint256& block_hash);
};

} // namespace llmq

#endif // BITCOIN_LLMQ_QUORUMCACHE_H
