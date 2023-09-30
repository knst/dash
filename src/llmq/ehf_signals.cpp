// Copyright (c) 2023 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <llmq/ehf_signals.h>
#include <llmq/utils.h>
#include <llmq/quorums.h>
#include <llmq/signing_shares.h>
#include <llmq/commitment.h>

#include <evo/specialtx.h>

#include <index/txindex.h> // g_txindex
//#include <scheduler.h>

#include <validation.h>
#include <txmempool.h>

namespace llmq {


//std::unique_ptr<CEHFSignalsHandler> ehfSignalsHandler;

CEHFSignalsHandler::CEHFSignalsHandler(CSigningManager& sigman, CSigSharesManager& shareman,
                                       CQuorumManager& qman, CTxMemPool& mempool,
                                       CMNHFManager& mnhfManager) :
    qman(qman),
    sigman(sigman),
    shareman(shareman),
    mempool(mempool),
    mnhfManager(mnhfManager)
    /*,
    scheduler(std::make_unique<CScheduler>()),
    scheduler_thread(std::make_unique<std::thread>(std::thread(util::TraceThread, "cl-schdlr", [&] { scheduler->serviceQueue(); })))
    */
{
}


CEHFSignalsHandler::~CEHFSignalsHandler()
{
    /*
    scheduler->stop();
    scheduler_thread->join();
    */
}

void CEHFSignalsHandler::Start()
{
    sigman.RegisterRecoveredSigsListener(this);
/*    scheduler->scheduleEvery([&]() {
        CheckActiveState();
        EnforceBestChainLock();
        // regularly retry signing the current chaintip as it might have failed before due to missing islocks
        TrySignChainTip();
    }, std::chrono::seconds{5});
    */
}

void CEHFSignalsHandler::Stop()
{
//    scheduler->stop();
    sigman.UnregisterRecoveredSigsListener(this);
}

void CEHFSignalsHandler::UpdatedBlockTip(const CBlockIndex* const pindexNew)
{
/*
    scheduler->scheduleFromNow([&]() {
        CheckActiveState();
        EnforceBestChainLock();
        TrySignChainTip();
        tryLockChainTipScheduled = false;
    }, std::chrono::seconds{0});
    */
    LogPrintf("bits: %d %d %d\n", Params().GetConsensus().vDeployments[Consensus::DEPLOYMENT_MN_RR].bit,
Params().GetConsensus().vDeployments[Consensus::DEPLOYMENT_V20].bit,
Params().GetConsensus().vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit);
    if (llmq::utils::IsV20Active(pindexNew)) {
        trySignEHFSignal(Params().GetConsensus().vDeployments[Consensus::DEPLOYMENT_MN_RR].bit, pindexNew);
    }
}

void CEHFSignalsHandler::trySignEHFSignal(int bit, const CBlockIndex* const pindex)
{
    LogPrintf("trySign ehf: %d at height=%d\n", bit, pindex->nHeight);
    if (!fMasternodeMode) {
        LogPrintf("try sign - not masternode\n");
        return;
    }

    /*
    if (!m_mn_sync.IsBlockchainSynced()) {
        return;
    }
    */

    const uint256 requestId = ::SerializeHash(std::make_pair(MNEHF_REQUESTID_PREFIX, int64_t{bit}));

    LogPrintf("request: %s bit=%d\n", requestId.ToString(), bit);
    const Consensus::LLMQType& llmqType = Params().GetConsensus().llmqTypeMnhf;
    const auto& llmq_params_opt = llmq::GetLLMQParams(llmqType);
    if (!llmq_params_opt.has_value()) {
        LogPrintf("try sign - llmq params doesn't exist\n");
        return;
    }
    auto quorum = sigman.SelectQuorumForSigning(llmq_params_opt.value(), qman, requestId);
    if (!quorum) {
        LogPrintf("EHF - No active quorum\n");
    }

    LogPrintf("quorum: %lld\n", quorum.get());
    LogPrintf("quorum->qc: %lld\n", quorum->qc.get());
    LogPrintf("quorum hash: %s\n", quorum->qc->quorumHash.ToString());
/*
    LogPrintf("quorum: %lld\n", quorum.get());
    LogPrintf("quorum: %lld\n", quorum.get());
    LogPrintf("quorum: %lld\n", quorum.get());
    LogPrintf("quorum: %lld\n", quorum.get());
    LogPrintf("quorum: %lld\n", quorum.get());
    LogPrintf("quorum->qc: %lld\n", quorum->qc.get());
    LogPrintf("quorum->qc: %lld\n", quorum->qc.get());
    LogPrintf("quorum->qc: %lld\n", quorum->qc.get());
    LogPrintf("quorum->qc: %lld\n", quorum->qc.get());
    LogPrintf("quorum->qc: %lld\n", quorum->qc.get());
    LogPrintf("quorum->qc: %lld\n", quorum->qc.get());
    LogPrintf("quorum->qc: %lld\n", quorum->qc.get());
    LogPrintf("quorum hash: %s\n", quorum->qc->quorumHash.ToString());
    */
    MNHFTxPayload mnhfPayload;
    mnhfPayload.signal.versionBit = bit;
    mnhfPayload.signal.quorumHash = quorum->qc->quorumHash;

    CMutableTransaction tx;
    SetTxPayload(tx, mnhfPayload);
    const uint256 msgHash = tx.GetHash();

    sigman.AsyncSignIfMember(llmqType, shareman, requestId, msgHash);
}

void CEHFSignalsHandler::HandleNewRecoveredSig(const CRecoveredSig& recoveredSig)
{
    if (g_txindex) {
        g_txindex->BlockUntilSyncedToCurrentChain();
    }
    // TODO knst do nothing

    LogPrintf("Handle new recovered sig: %s!\n", recoveredSig.ToJson().write());


    int bit = Consensus::Params().vDeployments[Consensus::DEPLOYMENT_MN_RR].bit;
    const uint256 requestId = ::SerializeHash(std::make_pair(MNEHF_REQUESTID_PREFIX, int64_t{bit}));

    LogPrintf("while expecting: request: %s bit=%d\n", requestId.ToString(), bit);
    if (requestId != recoveredSig.getId()) return;

    MNHFTxPayload mnhfPayload;
    mnhfPayload.signal.versionBit = bit;
    mnhfPayload.signal.quorumHash = recoveredSig.getQuorumHash();
    mnhfPayload.signal.sig = recoveredSig.sig.Get();

    CMutableTransaction tx;
    SetTxPayload(tx, mnhfPayload);
//    const uint256 msgHash = tx.GetHash();
//    tx.
    TxValidationState state;
    LogPrintf("send tx: %s\n", tx.GetHash().ToString());
    AcceptToMemoryPool(::ChainstateActive(), mempool, state, MakeTransactionRef(std::move(tx)),
                        false, 10);
}
} // namespace llmq
