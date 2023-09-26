// Copyright (c) 2023 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <llmq/ehf_signals.h>

//#include <scheduler.h>

namespace llmq;


//std::unique_ptr<CEHFSignalsHandler> ehfSignalsHandler;

CEHFSignalsHandler::CEHFSignalsHandler(CMNHFManager& mnhfManager) :
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

void CEHFSignalsHandler::UpdatedBlockTip()
{
    // don't call TrySignChainTip directly but instead let the scheduler call it. This way we ensure that cs_main is
    // never locked and TrySignChainTip is not called twice in parallel. Also avoids recursive calls due to
    // EnforceBestChainLock switching chains.
    // atomic[If tryLockChainTipScheduled is false, do (set it to true] and schedule signing).
    if (bool expected = false; tryLockChainTipScheduled.compare_exchange_strong(expected, true)) {
        scheduler->scheduleFromNow([&]() {
            CheckActiveState();
            EnforceBestChainLock();
            TrySignChainTip();
            tryLockChainTipScheduled = false;
        }, std::chrono::seconds{0});
    }
}

