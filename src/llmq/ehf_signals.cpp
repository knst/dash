// Copyright (c) 2023 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <llmq/ehf_signals.h>
#include <llmq/utils.h>
#include <llmq/quorums.h>
#include <llmq/signing_shares.h>
#include <llmq/commitment.h>


#include <evo/mnhftx.h>
#include <evo/specialtx.h>

#include <index/txindex.h> // g_txindex

#include <primitives/transaction.h>
#include <spork.h>
#include <txmempool.h>
#include <validation.h>

namespace llmq {


CEHFSignalsHandler::CEHFSignalsHandler(CChainState& chainstate, CConnman& connman,
                                       CSigningManager& sigman, CSigSharesManager& shareman,
                                       CSporkManager& sporkman, CQuorumManager& qman, CTxMemPool& mempool,
                                       CMNHFManager& mnhfManager) :
    chainstate(chainstate),
    connman(connman),
    sigman(sigman),
    shareman(shareman),
    sporkman(sporkman),
    qman(qman),
    mempool(mempool),
    mnhfManager(mnhfManager)
{
    sigman.RegisterRecoveredSigsListener(this);
}


CEHFSignalsHandler::~CEHFSignalsHandler()
{
    sigman.UnregisterRecoveredSigsListener(this);
}

void CEHFSignalsHandler::UpdatedBlockTip(const CBlockIndex* const pindexNew)
{
    if (!fMasternodeMode) {
        return;
    }

    if (!llmq::utils::IsV20Active(pindexNew)) {
        return;
    }

    if (sporkman.IsSporkActive(SPORK_24_MN_RR_READY)) {
        trySignEHFSignal(Params().GetConsensus().vDeployments[Consensus::DEPLOYMENT_MN_RR].bit, pindexNew);
    }
}

void CEHFSignalsHandler::trySignEHFSignal(int bit, const CBlockIndex* const pindex)
{
    LogPrintf("knst trySign ehf: %d at height=%d\n", bit, pindex->nHeight);

    /*
    if (!m_mn_sync.IsBlockchainSynced()) {
        return;
    }
    */

    MNHFTxPayload mnhfPayload;
    mnhfPayload.signal.versionBit = bit;
    const uint256 requestId = mnhfPayload.GetRequestId();

    LogPrintf("knst request: %s bit=%d\n", requestId.ToString(), bit);
    const Consensus::LLMQType& llmqType = Params().GetConsensus().llmqTypeMnhf;
    const auto& llmq_params_opt = llmq::GetLLMQParams(llmqType);
    if (!llmq_params_opt.has_value()) {
        LogPrintf("knst try sign - llmq params doesn't exist\n");
        return;
    }
    if (sigman.HasRecoveredSigForId(llmqType, requestId)) {
        ids.insert(requestId);

        // no need to sign one more
        return;
    }

    const auto quorum = sigman.SelectQuorumForSigning(llmq_params_opt.value(), qman, requestId);
    if (!quorum) {
        LogPrintf("knst EHF - No active quorum\n");
        return;
    }

    LogPrintf("knst quorum: %lld\n", quorum.get());
    LogPrintf("knst quorum->qc: %lld\n", quorum->qc.get());
    LogPrintf("knst quorum hash: %s\n", quorum->qc->quorumHash.ToString());

    mnhfPayload.signal.quorumHash = quorum->qc->quorumHash;
    const uint256 msgHash = mnhfPayload.PrepareTx().GetHash();

    ids.insert(requestId);
    sigman.AsyncSignIfMember(llmqType, shareman, requestId, msgHash);
}

void CEHFSignalsHandler::HandleNewRecoveredSig(const CRecoveredSig& recoveredSig)
{
    if (g_txindex) {
        g_txindex->BlockUntilSyncedToCurrentChain();
    }

    if (ids.find(recoveredSig.getId()) == ids.end()) {
        // Do nothing, it's not for this handler
        return;
    }

    MNHFTxPayload mnhfPayload;
    mnhfPayload.signal.versionBit = Params().GetConsensus().vDeployments[Consensus::DEPLOYMENT_MN_RR].bit;

    const uint256 expectedId = mnhfPayload.GetRequestId();
    LogPrintf("CEHFSignalsHandler::HandleNewRecoveredSig expecting ID=%s received=%s\n", expectedId().ToString(), recoveredSig.getId().ToString());
    if (recoveredSig.getId() != mnhfPayload.GetRequestId()) {
        // there's nothing interesting for CEHFSignalsHandler
        LogPrintf("CEHFSignalsHandler::HandleNewRecoveredSig checking if it is MN_RR release. Expected: %s\n", mnhfPayload.GetRequestId().ToString());
        return;
    }

    mnhfPayload.signal.quorumHash = recoveredSig.getQuorumHash();
    mnhfPayload.signal.sig = recoveredSig.sig.Get();

    CMutableTransaction tx = mnhfPayload.PrepareTx();

    {
        CTransactionRef tx_to_sent = MakeTransactionRef(std::move(tx));
        LogPrintf("CEHFSignalsHandler::HandleNewRecoveredSig Special EHF TX is going to sent hash=%s\n", tx_to_sent->GetHash().ToString());
        LOCK(cs_main);
        TxValidationState state;
        if (AcceptToMemoryPool(chainstate, mempool, state, tx_to_sent, /* bypass_limits=*/ false, /* nAbsurdFee=*/ 0)) {
            connman.RelayTransaction(*tx_to_sent);
        } else {
            LogPrintf("CEHFSignalsHandler::HandleNewRecoveredSig -- AcceptToMemoryPool failed: %s\n", state.ToString());
        }
    }
}
} // namespace llmq
