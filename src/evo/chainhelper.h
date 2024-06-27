// Copyright (c) 2024 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_EVO_CHAINHELPER_H
#define BITCOIN_EVO_CHAINHELPER_H

#include <amount.h>
#include <sync.h>
#include <threadsafety.h>

#include <memory>
#include <optional>

class BlockValidationState;
class CBlock;
class CBlockIndex;
class CCreditPoolManager;
class CCoinsViewCache;
class CDeterministicMNManager;
class ChainstateManager;
class CMNHFManager;
class CMNPaymentsProcessor;
class CMasternodeSync;
class CGovernanceManager;
class CTransaction;
class CSpecialTxProcessor;
class CSporkManager;
class TxValidationState;
struct MNListUpdates;

namespace Consensus { struct Params; }
namespace llmq {
class CChainLocksHandler;
class CQuorumBlockProcessor;
class CQuorumManager;
}

extern RecursiveMutex cs_main;

class CChainstateHelper
{
public:
    explicit CChainstateHelper(CCreditPoolManager& cpoolman, CDeterministicMNManager& dmnman, CMNHFManager& mnhfman, CGovernanceManager& govman,
                               llmq::CQuorumBlockProcessor& qblockman, const ChainstateManager& chainman, const Consensus::Params& consensus_params,
                               const CMasternodeSync& mn_sync, const CSporkManager& sporkman, const llmq::CChainLocksHandler& clhandler,
                               const llmq::CQuorumManager& qman);
    ~CChainstateHelper();

    CChainstateHelper() = delete;
    CChainstateHelper(const CChainstateHelper&) = delete;

    bool CheckSpecialTx(const CTransaction& tx, const CBlockIndex* pindexPrev, const CCoinsViewCache& view, bool check_sigs, TxValidationState& state)
        EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    bool UndoSpecialTxsInBlock(const CBlock& block, const CBlockIndex* pindex, std::optional<MNListUpdates>& updatesRet)
        EXCLUSIVE_LOCKS_REQUIRED(cs_main);
    bool ProcessSpecialTxsInBlock(const CBlock& block, const CBlockIndex* pindex, const CCoinsViewCache& view, bool fJustCheck,
                                  bool fCheckCbTxMerkleRoots, BlockValidationState& state, std::optional<MNListUpdates>& updatesRet)
        EXCLUSIVE_LOCKS_REQUIRED(cs_main);
    bool CheckCreditPoolDiffForBlock(const CBlock& block, const CBlockIndex* pindex, const CAmount blockSubsidy, BlockValidationState& state)
        EXCLUSIVE_LOCKS_REQUIRED(cs_main);

public:
    const std::unique_ptr<CMNPaymentsProcessor> mn_payments;

private:
    const std::unique_ptr<CSpecialTxProcessor> m_special_tx;
};

#endif // BITCOIN_EVO_CHAINHELPER_H
