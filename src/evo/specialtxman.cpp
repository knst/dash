// Copyright (c) 2018-2022 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <evo/specialtxman.h>

#include <chainparams.h>
#include <consensus/validation.h>
#include <evo/cbtx.h>
#include <evo/deterministicmns.h>
#include <evo/mnhftx.h>
#include <evo/providertx.h>
#include <evo/assetlocktx.h>
#include <hash.h>
#include <llmq/blockprocessor.h>
#include <llmq/commitment.h>
#include <llmq/utils.h>
#include <primitives/block.h>
#include <validation.h>

bool CheckSpecialTx(const CTransaction& tx, const CBlockIndex* pindexPrev, TxValidationState& state, const CCoinsViewCache& view, bool check_sigs)
{
    AssertLockHeld(cs_main);

    if (tx.nVersion != 3 || tx.nType == TRANSACTION_NORMAL)
        return true;

    if (pindexPrev && pindexPrev->nHeight + 1 < Params().GetConsensus().DIP0003Height) {
        return state.Invalid(TxValidationResult::TX_BAD_SPECIAL, "bad-tx-type");
    }

    try {
        switch (tx.nType) {
        case TRANSACTION_PROVIDER_REGISTER:
            return CheckProRegTx(tx, pindexPrev, state, view, check_sigs);
        case TRANSACTION_PROVIDER_UPDATE_SERVICE:
            return CheckProUpServTx(tx, pindexPrev, state, check_sigs);
        case TRANSACTION_PROVIDER_UPDATE_REGISTRAR:
            return CheckProUpRegTx(tx, pindexPrev, state, view, check_sigs);
        case TRANSACTION_PROVIDER_UPDATE_REVOKE:
            return CheckProUpRevTx(tx, pindexPrev, state, check_sigs);
        case TRANSACTION_COINBASE:
            return CheckCbTx(tx, pindexPrev, state);
        case TRANSACTION_QUORUM_COMMITMENT:
            return llmq::CheckLLMQCommitment(tx, pindexPrev, state);
        case TRANSACTION_MNHF_SIGNAL:
            return VersionBitsState(::ChainActive().Tip(), Params().GetConsensus(), Consensus::DEPLOYMENT_DIP0024, versionbitscache) == ThresholdState::ACTIVE && CheckMNHFTx(tx, pindexPrev, state);
        case TRANSACTION_ASSET_LOCK:
        case TRANSACTION_ASSET_UNLOCK:
            if (!llmq::utils::IsV20Active(pindexPrev)) {
                return state.Invalid(TxValidationResult::TX_CONSENSUS, "v20-not-active");
            }
            return CheckAssetLockUnlockTx(tx, pindexPrev, state);
        }
    } catch (const std::exception& e) {
        LogPrintf("%s -- failed: %s\n", __func__, e.what());
        return state.Invalid(TxValidationResult::TX_CONSENSUS, "failed-check-special-tx");
    }

    return state.Invalid(TxValidationResult::TX_BAD_SPECIAL, "bad-tx-type-check");
}

bool ProcessSpecialTx(const CTransaction& tx, const CBlockIndex* pindex, TxValidationState& state)
{
    if (tx.nVersion != 3 || tx.nType == TRANSACTION_NORMAL) {
        return true;
    }

    switch (tx.nType) {
    case TRANSACTION_ASSET_LOCK:
    case TRANSACTION_ASSET_UNLOCK:
        return true; // handled per block (during cb)
    case TRANSACTION_PROVIDER_REGISTER:
    case TRANSACTION_PROVIDER_UPDATE_SERVICE:
    case TRANSACTION_PROVIDER_UPDATE_REGISTRAR:
    case TRANSACTION_PROVIDER_UPDATE_REVOKE:
        return true; // handled in batches per block
    case TRANSACTION_COINBASE:
        return true; // nothing to do
    case TRANSACTION_QUORUM_COMMITMENT:
        return true; // handled per block
    case TRANSACTION_MNHF_SIGNAL:
        return true; // handled per block
    }

    return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-tx-type-proc");
}

bool UndoSpecialTx(const CTransaction& tx, const CBlockIndex* pindex)
{
    if (tx.nVersion != 3 || tx.nType == TRANSACTION_NORMAL) {
        return true;
    }

    switch (tx.nType) {
    case TRANSACTION_ASSET_LOCK:
    case TRANSACTION_ASSET_UNLOCK:
        return true; // handled per block (during cb)
    case TRANSACTION_PROVIDER_REGISTER:
    case TRANSACTION_PROVIDER_UPDATE_SERVICE:
    case TRANSACTION_PROVIDER_UPDATE_REGISTRAR:
    case TRANSACTION_PROVIDER_UPDATE_REVOKE:
        return true; // handled in batches per block
    case TRANSACTION_COINBASE:
        return true; // nothing to do
    case TRANSACTION_QUORUM_COMMITMENT:
        return true; // handled per block
    case TRANSACTION_MNHF_SIGNAL:
        return true; // handled per block
    }

    return false;
}

bool ProcessSpecialTxsInBlock(const CBlock& block, const CBlockIndex* pindex, llmq::CQuorumBlockProcessor& quorum_block_processor, const llmq::CChainLocksHandler& chainlock_handler,
                              BlockValidationState& state, const CCoinsViewCache& view, bool fJustCheck, bool fCheckCbTxMerleRoots)
{
    AssertLockHeld(cs_main);

    try {
        static int64_t nTimeLoop = 0;
        static int64_t nTimeQuorum = 0;
        static int64_t nTimeDMN = 0;
        static int64_t nTimeMerkle = 0;

        int64_t nTime1 = GetTimeMicros();

        for (const auto& ptr_tx : block.vtx) {
            TxValidationState tx_state;
            // At this moment CheckSpecialTx() and ProcessSpecialTx() may fail by 2 possible ways:
            // consensus failures and "TX_BAD_SPECIAL"
            if (!CheckSpecialTx(*ptr_tx, pindex->pprev, tx_state, view, fCheckCbTxMerleRoots)) {
                assert(tx_state.GetResult() == TxValidationResult::TX_CONSENSUS || tx_state.GetResult() == TxValidationResult::TX_BAD_SPECIAL);
                return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, tx_state.GetRejectReason(),
                                 strprintf("Special Transaction check failed (tx hash %s) %s", ptr_tx->GetHash().ToString(), tx_state.GetDebugMessage()));
            }
            if (!ProcessSpecialTx(*ptr_tx, pindex, tx_state)) {
                assert(tx_state.GetResult() == TxValidationResult::TX_CONSENSUS || tx_state.GetResult() == TxValidationResult::TX_BAD_SPECIAL);
                return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, tx_state.GetRejectReason(),
                                 strprintf("Process Special Transaction failed (tx hash %s) %s", ptr_tx->GetHash().ToString(), tx_state.GetDebugMessage()));
            }
        }

        int64_t nTime2 = GetTimeMicros();
        nTimeLoop += nTime2 - nTime1;
        LogPrint(BCLog::BENCHMARK, "        - Loop: %.2fms [%.2fs]\n", 0.001 * (nTime2 - nTime1), nTimeLoop * 0.000001);

        if (!quorum_block_processor.ProcessBlock(block, pindex, state, fJustCheck, fCheckCbTxMerleRoots)) {
            // pass the state returned by the function above
            return false;
        }

        int64_t nTime3 = GetTimeMicros();
        nTimeQuorum += nTime3 - nTime2;
        LogPrint(BCLog::BENCHMARK, "        - quorumBlockProcessor: %.2fms [%.2fs]\n", 0.001 * (nTime3 - nTime2), nTimeQuorum * 0.000001);

        if (!deterministicMNManager->ProcessBlock(block, pindex, state, view, fJustCheck)) {
            // pass the state returned by the function above
            return false;
        }

        int64_t nTime4 = GetTimeMicros();
        nTimeDMN += nTime4 - nTime3;
        LogPrint(BCLog::BENCHMARK, "        - deterministicMNManager: %.2fms [%.2fs]\n", 0.001 * (nTime4 - nTime3), nTimeDMN * 0.000001);

        if (fCheckCbTxMerleRoots && !CheckCbTxMerkleRoots(block, pindex, quorum_block_processor, state, view)) {
            // pass the state returned by the function above
            return false;
        }

        if (!CheckCbTxBestChainlock(block, pindex, chainlock_handler, state)) {
            // pass the state returned by the function above
            return false;
        }

        int64_t nTime5 = GetTimeMicros();
        nTimeMerkle += nTime5 - nTime4;
        LogPrint(BCLog::BENCHMARK, "        - CheckCbTxMerkleRoots: %.2fms [%.2fs]\n", 0.001 * (nTime5 - nTime4), nTimeMerkle * 0.000001);
    } catch (const std::exception& e) {
        LogPrintf("%s -- failed: %s\n", __func__, e.what());
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "failed-procspectxsinblock");
    }

    return true;
}

bool UndoSpecialTxsInBlock(const CBlock& block, const CBlockIndex* pindex, llmq::CQuorumBlockProcessor& quorum_block_processor)
{
    AssertLockHeld(cs_main);

    try {
        for (int i = (int)block.vtx.size() - 1; i >= 0; --i) {
            const CTransaction& tx = *block.vtx[i];
            if (!UndoSpecialTx(tx, pindex)) {
                return false;
            }
        }

        if (!deterministicMNManager->UndoBlock(block, pindex)) {
            return false;
        }

        if (!quorum_block_processor.UndoBlock(block, pindex)) {
            return false;
        }
    } catch (const std::exception& e) {
        return error(strprintf("%s -- failed: %s\n", __func__, e.what()).c_str());
    }

    return true;
}
