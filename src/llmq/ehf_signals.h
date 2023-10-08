// Copyright (c) 2023 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_LLMQ_EHF_SIGNALS_H
#define BITCOIN_LLMQ_EHF_SIGNALS_H

#include <llmq/signing.h>

#include <set>

class CBlockIndex;
class CChainState;
class CConnman;
class CMNHFManager;
class CSporkManager;
class CTxMemPool;

namespace llmq
{
class CQuorumManager;
class CSigSharesManager;
class CSigningManager;

class CEHFSignalsHandler : public CRecoveredSigsListener
{
private:
    CChainState& chainstate;
    CConnman& connman;
    CSigningManager& sigman;
    CSigSharesManager& shareman;
    CSporkManager& sporkman;
    CQuorumManager& qman;
    CTxMemPool& mempool;
    CMNHFManager& mnhfManager;

    /**
     * keep freshly generated IDs for easier filter sigs in HandleNewRecoveredSig
     */
    std::set<uint256> ids;
public:
    explicit CEHFSignalsHandler(CChainState& chainstate, CConnman& connman,
                                CSigningManager& sigman, CSigSharesManager& shareman,
                                CSporkManager& sporkman, CQuorumManager& qman, CTxMemPool& mempool,
                                CMNHFManager& mnhfManager);
    ~CEHFSignalsHandler();


    /**
     * Since Tip is updated it could be a time to generate EHF Signal
     */
    void UpdatedBlockTip(const CBlockIndex* const pindexNew);

    void HandleNewRecoveredSig(const CRecoveredSig& recoveredSig) override LOCKS_EXCLUDED(cs);

private:
    void trySignEHFSignal(int bit, const CBlockIndex* const pindex);

};

} // namespace llmq

#endif // BITCOIN_LLMQ_EHF_SIGNALS_H
