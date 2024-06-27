// Copyright (c) 2024 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <evo/chainhelper.h>

#include <consensus/params.h>
#include <evo/specialtxman.h>
#include <masternode/payments.h>

CChainstateHelper::CChainstateHelper(CCreditPoolManager& cpoolman, CDeterministicMNManager& dmnman, CMNHFManager& mnhfman, CGovernanceManager& govman,
                                     llmq::CQuorumBlockProcessor& qblockman, const ChainstateManager& chainman, const Consensus::Params& consensus_params,
                                     const CMasternodeSync& mn_sync, const CSporkManager& sporkman, const llmq::CChainLocksHandler& clhandler,
                                     const llmq::CQuorumManager& qman)
    : mn_payments{std::make_unique<CMNPaymentsProcessor>(dmnman, govman, chainman, consensus_params, mn_sync, sporkman)},
      m_special_tx{std::make_unique<CSpecialTxProcessor>(cpoolman, dmnman, mnhfman, qblockman, chainman, consensus_params, clhandler, qman)}
{}

CChainstateHelper::~CChainstateHelper() = default;

bool CChainstateHelper::CheckSpecialTx(const CTransaction& tx, const CBlockIndex* pindexPrev, const CCoinsViewCache& view, bool check_sigs, TxValidationState& state)
{
    AssertLockHeld(cs_main);

    return m_special_tx->CheckSpecialTx(tx, pindexPrev, view, check_sigs, state);
}

bool CChainstateHelper::UndoSpecialTxsInBlock(const CBlock& block, const CBlockIndex* pindex, std::optional<MNListUpdates>& updatesRet)
{
    AssertLockHeld(cs_main);
    return m_special_tx->UndoSpecialTxsInBlock(block, pindex, updatesRet);
}

bool CChainstateHelper::ProcessSpecialTxsInBlock(const CBlock& block, const CBlockIndex* pindex, const CCoinsViewCache& view, bool fJustCheck,
                              bool fCheckCbTxMerkleRoots, BlockValidationState& state, std::optional<MNListUpdates>& updatesRet)
{
    AssertLockHeld(cs_main);
    return m_special_tx->ProcessSpecialTxsInBlock(block, pindex, view, fJustCheck, fCheckCbTxMerkleRoots, state, updatesRet);
}

bool CChainstateHelper::CheckCreditPoolDiffForBlock(const CBlock& block, const CBlockIndex* pindex, const CAmount blockSubsidy, BlockValidationState& state)
{
    AssertLockHeld(cs_main);
    return m_special_tx->CheckCreditPoolDiffForBlock(block, pindex, blockSubsidy, state);
}
