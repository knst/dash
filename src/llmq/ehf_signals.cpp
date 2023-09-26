// Copyright (c) 2023 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <llmq/ehf_signals.h>

//#include <scheduler.h>

namespace llmq {


//std::unique_ptr<CEHFSignalsHandler> ehfSignalsHandler;

CEHFSignalsHandler::CEHFSignalsHandler(CSigningManager& sigman, CSigSharesManager& shareman,
                                       CMNHFManager& mnhfManager) :
    sigman(sigman),
    shareman(shareman),
    mnhfManager(mnhfManager)
    /*,
    scheduler(std::make_unique<CScheduler>()),
    scheduler_thread(std::make_unique<std::thread>(std::thread(util::TraceThread, "cl-schdlr", [&] { scheduler->serviceQueue(); })))
    */
{
}


~CEHFSignalsHandler::CEHFSignalsHandler()
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
    if (IsV20Active(pindexNew)) {
        trySignEHFSignal(bit MN reward);
    }
}

void CEHFSignalsHandler::trySignEHFSignal(int bit)
{
    if (!fMasternodeMode) {
        return;
    }

    if (!m_mn_sync.IsBlockchainSynced()) {
        return;
    }

    uint256 requestId = ::SerializeHash(std::make_pair(MNHF_REQUESTID_PREFIX, pindex->nHeight));
    /*
    uint256 msgHash = pindex->GetBlockHash();

    {
        LOCK(cs);
        if (bestChainLock.getHeight() >= pindex->nHeight) {
            // might have happened while we didn't hold cs
            return;
        }
        lastSignedHeight = pindex->nHeight;
        lastSignedRequestId = requestId;
        lastSignedMsgHash = msgHash;
    }
    */

    sigman.AsyncSignIfMember(Params().GetConsensus().llmqTypeMNHF, shareman, requestId, msgHash);
}

