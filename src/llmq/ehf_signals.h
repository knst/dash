// Copyright (c) 2023 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_LLMQ_EHF_SIGNALS_H
#define BITCOIN_LLMQ_EHF_SIGNALS_H

#include <evo/mnhftx.h>

#include <crypto/common.h>
#include <llmq/signing.h>
namespace llmq
{
class CQuorumManager;
class CSigSharesManager;
class CSigningManager;

class CEHFSignalsHandler : public CRecoveredSigsListener
{
private:
    CSigningManager& sigman;
    CSigSharesManager& shareman;
    CQuorumManager& qman;
    CMNHFManager& mnhfManager;

/*    std::unique_ptr<CScheduler> scheduler;
    std::unique_ptr<std::thread> scheduler_thread;
    */
public:
    explicit CEHFSignalsHandler(CSigningManager& sigman, CSigSharesManager& shareman, CQuorumManager& qman, CMNHFManager& mnhfManager);
    ~CEHFSignalsHandler();

    void Start();
    void Stop();

    void UpdatedBlockTip(const CBlockIndex* const pindexNew);

    /*
    void BlockConnected(const std::shared_ptr<const CBlock>& pblock, const CBlockIndex* pindex) LOCKS_EXCLUDED(cs);
    void BlockDisconnected(const std::shared_ptr<const CBlock>& pblock, const CBlockIndex* pindexDisconnected) LOCKS_EXCLUDED(cs);
    void CheckActiveState() LOCKS_EXCLUDED(cs);
*/
    void HandleNewRecoveredSig(const CRecoveredSig& recoveredSig) override LOCKS_EXCLUDED(cs);

private:
    void trySignEHFSignal(int bit, const CBlockIndex* const pindex);
};

//extern std::unique_ptr<CEHFSignalsHandler> ehfSignalsHandler;
} // namespace llmq

#endif // BITCOIN_LLMQ_EHF_SIGNALS_H
