// Copyright (c) 2021-2022 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/validation.h>
#include <evo/mnhftx.h>
#include <evo/specialtx.h>
#include <llmq/commitment.h>
#include <llmq/signing.h>
#include <llmq/utils.h>
#include <llmq/quorums.h>

#include <chain.h>
#include <chainparams.h>
#include <validation.h>
#include <versionbits.h>

#include <algorithm>
#include <string>
#include <vector>

extern const std::string MNEHF_REQUESTID_PREFIX = "mnhf";
static const std::string DB_SIGNALS = "mnhf_s";

bool MNHFTx::Verify(const CBlockIndex* pQuorumIndex, const uint256& msgHash, TxValidationState& state) const
{
    if (versionBit >= VERSIONBITS_NUM_BITS) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-mnhf-nbit-out-of-bounds");
    }

    Consensus::LLMQType llmqType = Params().GetConsensus().llmqTypeMnhf;
    const auto& llmq_params_opt = llmq::GetLLMQParams(llmqType);
    if (!llmq_params_opt.has_value()) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-mnhf-quorum-type");
    }
    int signOffset{llmq_params_opt->dkgInterval};

    const uint256 requestId = ::SerializeHash(std::make_pair(MNEHF_REQUESTID_PREFIX, int64_t{versionBit}));

    if (!llmq::CSigningManager::VerifyRecoveredSig(llmqType, *llmq::quorumManager, pQuorumIndex->nHeight + signOffset, requestId, msgHash, sig)) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-mnhf-invalid");
    }
    return true;
}

bool CheckMNHFTx(const CTransaction& tx, const CBlockIndex* pindexPrev, TxValidationState& state) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    if (tx.nVersion != 3 || tx.nType != TRANSACTION_MNHF_SIGNAL) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-mnhf-type");
    }

    MNHFTxPayload mnhfTx;
    if (!GetTxPayload(tx, mnhfTx)) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-mnhf-payload");
    }

    if (mnhfTx.nVersion == 0 || mnhfTx.nVersion > MNHFTxPayload::CURRENT_VERSION) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-mnhf-version");
    }

    const CBlockIndex* pindexQuorum = g_chainman.m_blockman.LookupBlockIndex(mnhfTx.signal.quorumHash);
    if (!pindexQuorum) {
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-mnhf-quorum-hash");
    }

    if (pindexQuorum != pindexPrev->GetAncestor(pindexQuorum->nHeight)) {
        // not part of active chain
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-mnhf-quorum-hash");
    }

    // Copy transaction except `quorumSig` field to calculate hash
    CMutableTransaction tx_copy(tx);
    auto payload_copy = mnhfTx;
    payload_copy.signal.sig = CBLSSignature();
    SetTxPayload(tx_copy, payload_copy);
    uint256 msgHash = tx_copy.GetHash();

    if (!mnhfTx.signal.Verify(pindexQuorum, msgHash, state)) {
        // set up inside Verify
        return false;
    }

    return true;
}

static bool extractSignals(const CBlock& block, const CBlockIndex* const pindex, std::vector<uint8_t>& signals_to_process, BlockValidationState& state) {

    // we skip the coinbase
    for (int i = 1; i < (int)block.vtx.size(); i++) {
        const CTransaction& tx = *block.vtx[i];

        if (tx.nVersion != 3 || tx.nType != TRANSACTION_MNHF_SIGNAL) {
            // only interested in special TXs 'TRANSACTION_MNHF_SIGNAL'
            continue;
        }

        TxValidationState tx_state;
        if (!CheckMNHFTx(tx, pindex, tx_state)) {
            return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, tx_state.GetRejectReason(), tx_state.GetDebugMessage());
        }

        MNHFTxPayload mnhfTx;
        if (!GetTxPayload(tx, mnhfTx)) {
            return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-mnhf-tx-payload");
        }
        signals_to_process.push_back(mnhfTx.signal.versionBit);
    }

    // Checking that there's no any duplicates...
    std::sort(signals_to_process.begin(), signals_to_process.end());
    const auto it = std::unique(signals_to_process.begin(), signals_to_process.end());
    if (std::distance(signals_to_process.begin(), it) != signals_to_process.size()) {
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-mnhf-duplicates");
    }

    return true;
}

bool CMNHFManager::ProcessBlock(const CBlock& block, const CBlockIndex* const pindex, bool fJustCheck, BlockValidationState& state)
{
    try {
        std::vector<uint8_t> new_signals;
        if (!extractSignals(block, pindex, new_signals, state)) {
            // state is set inside extractSignals
            return false;
        }
        if (new_signals.empty()) {
            return true;
        }

        Signals signals = GetFromCache(pindex);
        int mined_height = pindex->nHeight;

        // Extra validation of signals to be sure that it can succeed
        for (const auto& versionBit : new_signals) {
            // CheckMNHFTx() should validate that bit is not zero
            assert(versionBit != 0);

            LogPrintf("%s: add mnft bit=%d block:%s number of known signals:%lld\n", __func__, versionBit, pindex->GetBlockHash().ToString(), signals.size());
            if (signals.find(versionBit) != signals.end()) {
                return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-mnhf-duplicate");
            }

            if (!Params().UpdateMNActivationParam(versionBit, mined_height, pindex->GetMedianTimePast(), true /* fJustCheck */)) {
                return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-mnhf-non-mn-fork");
            }
        }
        if (fJustCheck) {
            // We are done, no need actually update any params
            return true;
        }
        for (const auto& versionBit : new_signals) {
            signals.insert({versionBit, mined_height});

            if (!Params().UpdateMNActivationParam(versionBit, mined_height, pindex->GetMedianTimePast(), false /* fJustCheck */)) {
                // it should not ever fail - all checks are done above
                assert(false);
            }

        }

        AddToCache(signals, pindex);
        return true;
    } catch (const std::exception& e) {
        LogPrintf("%s -- failed: %s\n", __func__, e.what());
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "failed-proc-mnhf-inblock");
    }
}

bool CMNHFManager::UndoBlock(const CBlock& block, const CBlockIndex* const pindex)
{
    std::vector<uint8_t> excluded_signals;
    BlockValidationState state;
    if (!extractSignals(block, pindex, excluded_signals, state)) {
        return false;
    }
    if (excluded_signals.empty()) {
        return true;
    }

    const Signals signals = GetFromCache(pindex->pprev);
    for (const auto& versionBit : excluded_signals) {
        // CheckMNHFTx() should validate that bit is not zero
        assert(versionBit != 0);

        LogPrintf("%s: exclude mnft bit=%d block:%s number of known signals:%lld\n", __func__, versionBit, pindex->GetBlockHash().ToString(), signals.size());
        assert(signals.find(versionBit) != signals.end());

        bool update_ret = !Params().UpdateMNActivationParam(versionBit, 0, pindex->GetMedianTimePast(), false /* fJustCheck */);
        assert(update_ret);
    }

    return true;
}

void CMNHFManager::UpdateChainParams(const CBlockIndex* const pindex, const CBlockIndex* const pindexOld)
{
    Signals signals_old{GetFromCache(pindexOld)};
    for (const auto& signal: signals_old) {
        uint8_t versionBit = signal.first;
        assert(versionBit < VERSIONBITS_NUM_BITS);

        LogPrintf("%s: unload mnft bit=%d block:%s number of known signals:%lld\n", __func__, versionBit, pindex->GetBlockHash().ToString(), signals_old.size());

        bool update_ret = !Params().UpdateMNActivationParam(versionBit, 0, pindex->GetMedianTimePast(), true);
        assert(update_ret);
    }

    Signals signals{GetFromCache(pindex)};
    for (const auto& signal: signals) {
        uint8_t versionBit = signal.first;
        int value = signal.second;
        assert(versionBit < VERSIONBITS_NUM_BITS);

        LogPrintf("%s: load mnft bit=%d block:%s number of known signals:%lld\n", __func__, versionBit, pindex->GetBlockHash().ToString(), signals.size());

        bool update_ret = !Params().UpdateMNActivationParam(versionBit, value, pindex->GetMedianTimePast(), true);
        assert(update_ret);
    }
}

CMNHFManager::Signals CMNHFManager::GetFromCache(const CBlockIndex* const pindex)
{
    if (pindex == nullptr) return {};
    const uint256& blockHash = pindex->GetBlockHash();
    Signals signals{};
    {
        LOCK(cs_cache);
        if (mnhfCache.get(blockHash, signals)) {
            return signals;
        }
    }
    if (VersionBitsState(pindex->pprev, Params().GetConsensus(), Consensus::DEPLOYMENT_V20, versionbitscache) != ThresholdState::ACTIVE) {
        LOCK(cs_cache);
        mnhfCache.insert(blockHash, {});
        return signals;
    }
    m_evoDb.Read(std::make_pair(DB_SIGNALS, blockHash), signals);
    mnhfCache.insert(blockHash, signals);
    return signals;
}

void CMNHFManager::AddToCache(const Signals& signals, const CBlockIndex* const pindex)
{
    const uint256& blockHash = pindex->GetBlockHash();
    {
        LOCK(cs_cache);
        mnhfCache.insert(blockHash, signals);
    }
    m_evoDb.Write(std::make_pair(DB_SIGNALS, blockHash), signals);
}

std::string MNHFTx::ToString() const
{
    return strprintf("MNHFTx(versionBit=%d, quorumHash=%s, sig=%s)",
                     versionBit, quorumHash.ToString(), sig.ToString());
}
std::string MNHFTxPayload::ToString() const
{
    return strprintf("MNHFTxPayload(nVersion=%d, signal=%s)",
                     nVersion, signal.ToString());
}
