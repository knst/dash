// Copyright (c) 2018-2026 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <llmq/quorumcache.h>

#include <evo/evodb.h>
#include <llmq/commitment.h>
#include <llmq/options.h>
#include <llmq/utils.h>
#include <util/helpers.h>

#include <chain.h>
#include <chainparams.h>
#include <compat/endian.h>
#include <validation.h>

#include <limits>

namespace llmq {

QuorumResolutionCache::QuorumResolutionCache(const Consensus::Params& consensus_params)
{
    utils::InitQuorumsCache(mapQuorumsCache, consensus_params, /*limit_by_connections=*/false);
}

static const std::string DB_MINED_COMMITMENT = "q_mc";
static const std::string DB_MINED_COMMITMENT_BY_INVERSED_HEIGHT = "q_mcih";
static const std::string DB_MINED_COMMITMENT_BY_INVERSED_HEIGHT_Q_INDEXED = "q_mcihi";

static const std::string DB_BEST_BLOCK_UPGRADE = "q_bbu2";

// We store a mapping from minedHeight->quorumHeight in the DB
// minedHeight is inversed so that entries are traversable in reversed order
static std::tuple<std::string, Consensus::LLMQType, uint32_t> BuildInversedHeightKey(Consensus::LLMQType llmqType, int nMinedHeight)
{
    // nMinedHeight must be converted to big endian to make it comparable when serialized
    return std::make_tuple(DB_MINED_COMMITMENT_BY_INVERSED_HEIGHT, llmqType, htobe32_internal(std::numeric_limits<uint32_t>::max() - nMinedHeight));
}

static std::tuple<std::string, Consensus::LLMQType, int, uint32_t> BuildInversedHeightKeyIndexed(Consensus::LLMQType llmqType, int nMinedHeight, int quorumIndex)
{
    // nMinedHeight must be converted to big endian to make it comparable when serialized
    return std::make_tuple(DB_MINED_COMMITMENT_BY_INVERSED_HEIGHT_Q_INDEXED, llmqType, quorumIndex, htobe32_internal(std::numeric_limits<uint32_t>::max() - nMinedHeight));
}

MinedCommitmentsStore::MinedCommitmentsStore(CEvoDB& evoDb, const ChainstateManager& chainman) :
    m_evoDb{evoDb},
    m_chainman{chainman}
{
    utils::InitQuorumsCache(mapMinedCommitmentBlockCache, m_chainman.GetConsensus());
}

bool MinedCommitmentsStore::HasMinedCommitment(Consensus::LLMQType llmqType, const uint256& quorumHash) const
{
    LOCK(::cs_main);
    return HasMinedCommitment(llmqType, quorumHash, m_chainman.ActiveChain());
}

bool MinedCommitmentsStore::HasMinedCommitment(Consensus::LLMQType llmqType, const uint256& quorumHash,
                                               const CChain& chain) const
{
    AssertLockHeld(::cs_main);

    uint256 mined_block_hash;
    bool cached;
    {
        LOCK(m_cache_cs);
        cached = mapMinedCommitmentBlockCache[llmqType].get(quorumHash, mined_block_hash);
    }
    if (!cached) {
        mined_block_hash = GetMinedCommitment(llmqType, quorumHash).second;
        // Do not negatively cache. Snapshot activation seeds EvoDB directly,
        // outside ProcessCommitment's normal cache-invalidation path.
        if (!mined_block_hash.IsNull()) {
            LOCK(m_cache_cs);
            mapMinedCommitmentBlockCache[llmqType].insert(quorumHash, mined_block_hash);
        }
    }

    const CBlockIndex* mined_block = m_chainman.m_blockman.LookupBlockIndex(mined_block_hash);
    return mined_block != nullptr && chain.Contains(mined_block);
}

std::pair<CFinalCommitment, uint256> MinedCommitmentsStore::GetMinedCommitment(Consensus::LLMQType llmqType,
                                                                               const uint256& quorumHash) const
{
    auto key = std::make_pair(DB_MINED_COMMITMENT, std::make_pair(llmqType, quorumHash));
    std::pair<CFinalCommitment, uint256> ret;
    if (!m_evoDb.Read(key, ret)) {
        return {CFinalCommitment{}, uint256::ZERO};
    }
    return ret;
}

// The returned quorums are in reversed order, so the most recent one is at index 0
std::vector<const CBlockIndex*> MinedCommitmentsStore::GetMinedCommitmentsUntilBlock(Consensus::LLMQType llmqType, gsl::not_null<const CBlockIndex*> pindex, size_t maxCount) const
{
    AssertLockNotHeld(m_evoDb.cs);
    LOCK(m_evoDb.cs);

    auto dbIt = m_evoDb.GetCurTransaction().NewIteratorUniquePtr();

    auto firstKey = BuildInversedHeightKey(llmqType, pindex->nHeight);
    auto lastKey = BuildInversedHeightKey(llmqType, 0);

    dbIt->Seek(firstKey);

    std::vector<const CBlockIndex*> ret;
    ret.reserve(maxCount);

    while (dbIt->Valid() && ret.size() < maxCount) {
        decltype(firstKey) curKey;
        int quorumHeight;
        if (!dbIt->GetKey(curKey) || curKey >= lastKey) {
            break;
        }
        if (std::get<0>(curKey) != DB_MINED_COMMITMENT_BY_INVERSED_HEIGHT || std::get<1>(curKey) != llmqType) {
            break;
        }

        if (uint32_t nMinedHeight = std::numeric_limits<uint32_t>::max() - be32toh_internal(std::get<2>(curKey));
                nMinedHeight > static_cast<uint32_t>(pindex->nHeight)) {
            break;
        }

        if (!dbIt->GetValue(quorumHeight)) {
            break;
        }

        const auto* pQuorumBaseBlockIndex = pindex->GetAncestor(quorumHeight);
        assert(pQuorumBaseBlockIndex);
        ret.emplace_back(pQuorumBaseBlockIndex);

        dbIt->Next();
    }

    return ret;
}

std::optional<const CBlockIndex*> MinedCommitmentsStore::GetLastMinedCommitmentsByQuorumIndexUntilBlock(Consensus::LLMQType llmqType, const CBlockIndex* pindex, int quorumIndex, size_t cycle) const
{
    AssertLockNotHeld(m_evoDb.cs);
    LOCK(m_evoDb.cs);

    auto dbIt = m_evoDb.GetCurTransaction().NewIteratorUniquePtr();

    auto firstKey = BuildInversedHeightKeyIndexed(llmqType, pindex->nHeight, quorumIndex);
    auto lastKey = BuildInversedHeightKeyIndexed(llmqType, 0, quorumIndex);

    size_t currentCycle = 0;

    dbIt->Seek(firstKey);

    while (dbIt->Valid()) {
        decltype(firstKey) curKey;
        int quorumHeight;
        if (!dbIt->GetKey(curKey) || curKey >= lastKey) {
            return std::nullopt;
        }
        if (std::get<0>(curKey) != DB_MINED_COMMITMENT_BY_INVERSED_HEIGHT_Q_INDEXED || std::get<1>(curKey) != llmqType) {
            return std::nullopt;
        }

        if (uint32_t nMinedHeight = std::numeric_limits<uint32_t>::max() - be32toh_internal(std::get<3>(curKey));
                nMinedHeight > static_cast<uint32_t>(pindex->nHeight)) {
            return std::nullopt;
        }

        if (!dbIt->GetValue(quorumHeight)) {
            return std::nullopt;
        }

        const auto* pQuorumBaseBlockIndex = pindex->GetAncestor(quorumHeight);
        assert(pQuorumBaseBlockIndex);

        if (currentCycle == cycle) {
            return std::make_optional(pQuorumBaseBlockIndex);
        }

        currentCycle++;

        dbIt->Next();
    }

    return std::nullopt;
}

std::vector<const CBlockIndex*> MinedCommitmentsStore::GetLastMinedCommitmentsPerQuorumIndexUntilBlock(
    Consensus::LLMQType llmqType, const CBlockIndex* pindex, size_t cycle) const
{
    const auto& llmq_params_opt = Params().GetLLMQ(llmqType);
    assert(llmq_params_opt.has_value());
    std::vector<const CBlockIndex*> ret;

    for (const auto quorumIndex : util::irange(llmq_params_opt->signingActiveQuorumCount)) {
        std::optional<const CBlockIndex*> q = GetLastMinedCommitmentsByQuorumIndexUntilBlock(llmqType, pindex, quorumIndex, cycle);
        if (q.has_value()) {
            ret.emplace_back(q.value());
        }
    }

    return ret;
}

std::vector<const CBlockIndex*> MinedCommitmentsStore::GetMinedCommitmentsIndexedUntilBlock(Consensus::LLMQType llmqType, const CBlockIndex* pindex, size_t maxCount) const
{
    std::vector<const CBlockIndex*> ret;

    size_t cycle = 0;

    while (ret.size() < maxCount) {
        std::vector<const CBlockIndex*> cycleRet = GetLastMinedCommitmentsPerQuorumIndexUntilBlock(llmqType, pindex, cycle);

        if (cycleRet.empty()) {
            return ret;
        }

        size_t needToCopy = maxCount - ret.size();
        std::copy_n(cycleRet.begin(), std::min(needToCopy, cycleRet.size()), std::back_inserter(ret));
        cycle++;
    }

    return ret;
}

// The returned quorums are in reversed order, so the most recent one is at index 0
std::map<Consensus::LLMQType, std::vector<const CBlockIndex*>> MinedCommitmentsStore::GetMinedAndActiveCommitmentsUntilBlock(gsl::not_null<const CBlockIndex*> pindex) const
{
    std::map<Consensus::LLMQType, std::vector<const CBlockIndex*>> ret;

    for (const auto& params : Params().GetConsensus().llmqs) {
        auto& commitments = ret[params.type];
        if (IsQuorumRotationEnabled(params, pindex)) {
            commitments = GetLastMinedCommitmentsPerQuorumIndexUntilBlock(params.type, pindex, 0);
        } else {
            commitments = GetMinedCommitmentsUntilBlock(params.type, pindex, params.signingActiveQuorumCount);
        }
    }

    return ret;
}

void MinedCommitmentsStore::WriteMinedCommitment(const CFinalCommitment& qc, const uint256& block_hash, int mined_height,
                                                 int quorum_base_height, bool rotation_enabled)
{
    m_evoDb.Write(std::make_pair(DB_MINED_COMMITMENT, std::make_pair(qc.llmqType, qc.quorumHash)),
                  std::make_pair(qc, block_hash));

    if (rotation_enabled) {
        m_evoDb.Write(BuildInversedHeightKeyIndexed(qc.llmqType, mined_height, int(qc.quorumIndex)), quorum_base_height);
    } else {
        m_evoDb.Write(BuildInversedHeightKey(qc.llmqType, mined_height), quorum_base_height);
    }

    WITH_LOCK(m_cache_cs, mapMinedCommitmentBlockCache[qc.llmqType].erase(qc.quorumHash));
}

void MinedCommitmentsStore::EraseMinedCommitment(const CFinalCommitment& qc, int mined_height, bool rotation_enabled)
{
    m_evoDb.Erase(std::make_pair(DB_MINED_COMMITMENT, std::make_pair(qc.llmqType, qc.quorumHash)));

    if (rotation_enabled) {
        m_evoDb.Erase(BuildInversedHeightKeyIndexed(qc.llmqType, mined_height, int(qc.quorumIndex)));
    } else {
        m_evoDb.Erase(BuildInversedHeightKey(qc.llmqType, mined_height));
    }

    WITH_LOCK(m_cache_cs, mapMinedCommitmentBlockCache[qc.llmqType].erase(qc.quorumHash));
}

void MinedCommitmentsStore::WriteBestBlockUpgrade(const uint256& block_hash)
{
    m_evoDb.Write(DB_BEST_BLOCK_UPGRADE, block_hash);
}

} // namespace llmq
