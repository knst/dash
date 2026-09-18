// Copyright (c) 2021-2025 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainlock/clsig.h>

#include <chain.h>
#include <chainlock/chainlock.h>
#include <chainparams.h>
#include <evo/cbtx.h>
#include <evo/specialtx.h>
#include <llmq/quorumsman.h>
#include <node/blockstorage.h>
#include <shutdown.h>

#include <algorithm>
#include <stdexcept>
#include <string_view>

namespace chainlock {
static constexpr std::string_view CLSIG_REQUESTID_PREFIX{"clsig"};
static constexpr size_t MAX_HISTORICAL_CARRIER_READS{16384};

CoinbaseChainLockReader::CoinbaseChainLockReader(const CChain& chain) :
    m_tip(chain.Tip())
{
}

std::optional<CoinbaseChainLock> CoinbaseChainLockReader::Read(int carrier_height)
{
    const auto* carrier = m_tip ? m_tip->GetAncestor(carrier_height) : nullptr;
    if (!carrier || carrier_height < Params().GetConsensus().V20Height) return std::nullopt;
    if (const auto it = m_cache.find(carrier_height); it != m_cache.end()) return it->second;
    if (ShutdownRequested()) throw std::runtime_error("ChainLock lookup interrupted");
    if (m_cache.size() >= MAX_HISTORICAL_CARRIER_READS)
        throw std::runtime_error("ChainLock disk-read budget exhausted");
    CBlock block;
    if (!node::ReadBlockFromDisk(block, carrier, Params().GetConsensus()) || block.vtx.empty()) {
        throw std::runtime_error("Historical ChainLock block data unavailable");
    }
    const auto cb = GetTxPayload<CCbTx>(*block.vtx[0]);
    if (!block.vtx[0]->IsCoinBase() || block.vtx[0]->nType != TRANSACTION_COINBASE || !cb ||
        cb->nVersion < CCbTx::Version::CLSIG_AND_BALANCE || cb->nHeight != carrier_height) {
        throw std::runtime_error("Invalid historical ChainLock coinbase");
    }
    if (cb->bestCLHeightDiff >= uint32_t(carrier_height)) {
        throw std::runtime_error("Invalid historical coinbase ChainLock height");
    }
    const auto chainlock = GetNonNullCoinbaseChainlock(block, carrier_height);
    if (!chainlock) {
        return m_cache.emplace(carrier_height, std::nullopt).first->second;
    }
    const int height = carrier_height - int(chainlock->second) - 1;
    const auto* signed_block = m_tip->GetAncestor(height);
    if (!signed_block) throw std::runtime_error("Invalid historical ChainLock block height");
    return m_cache
        .emplace(carrier_height,
                 CoinbaseChainLock{ChainLockSig{height, signed_block->GetBlockHash(), chainlock->first}, carrier})
        .first->second;
}

std::optional<CoinbaseChainLock> CoinbaseChainLockReader::Find(int minimum_height, int maximum_height)
{
    const int tip_height = m_tip ? m_tip->nHeight : -1;
    if (minimum_height < 0 || minimum_height > maximum_height || minimum_height >= tip_height)
        return std::nullopt;
    const int first_carrier = std::max(minimum_height + 1, Params().GetConsensus().V20Height);
    if (first_carrier > tip_height) return std::nullopt;

    // CheckCbTxBestChainlock enforces nondecreasing certified heights and
    // forbids null signatures after the first certificate on a validated chain.
    auto below_minimum = [&](int height) {
        const auto entry = Read(height);
        return !entry || entry->clsig.getHeight() < minimum_height;
    };
    int low = first_carrier;
    int high = first_carrier;
    int64_t step = 1;
    while (below_minimum(high)) {
        if (high == tip_height) return std::nullopt;
        low = high + 1;
        high = int(std::min<int64_t>(tip_height, int64_t(high) + step));
        step *= 2;
    }
    while (low < high) {
        const int middle = low + (high - low) / 2;
        if (below_minimum(middle)) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    const auto entry = Read(low);
    return entry && entry->clsig.getHeight() <= maximum_height ? entry : std::nullopt;
}

uint256 GenSigRequestId(const int32_t nHeight)
{
    return ::SerializeHash(std::make_pair(CLSIG_REQUESTID_PREFIX, nHeight));
}

llmq::VerifyRecSigStatus VerifyChainLock(const Consensus::Params& params, const CChain& chain,
                                         const llmq::CQuorumManager& qman, const chainlock::ChainLockSig& clsig)
{
    const auto llmqType = params.llmqTypeChainLocks;
    const uint256 request_id = GenSigRequestId(clsig.getHeight());

    return llmq::VerifyRecoveredSig(llmqType, chain, qman, clsig.getHeight(), request_id, clsig.getBlockHash(),
                                    clsig.getSig());
}

llmq::VerifyRecSigStatus VerifyChainLock(const Consensus::Params& params, const llmq::CQuorumManager& qman,
                                         const chainlock::ChainLockSig& clsig, const CBlockIndex* pindexStart)
{
    const auto llmqType = params.llmqTypeChainLocks;
    const uint256 request_id = GenSigRequestId(clsig.getHeight());

    return llmq::VerifyRecoveredSig(llmqType, qman, pindexStart, request_id, clsig.getBlockHash(), clsig.getSig());
}
} // namespace chainlock
