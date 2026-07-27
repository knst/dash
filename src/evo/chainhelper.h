// Copyright (c) 2024-2025 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_EVO_CHAINHELPER_H
#define BITCOIN_EVO_CHAINHELPER_H

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>

class CBlockIndex;
class CCreditPoolManager;
class CDeterministicMNManager;
class CEvoDB;
class ChainstateManager;
class CMasternodeSync;
class CMNHFManager;
class CMNPaymentsProcessor;
class CSpecialTxProcessor;
class CTransaction;
class uint256;
struct CCreditPool;
namespace chainlock {
class Chainlocks;
} // namespace chainlock
namespace governance {
class SuperblockManager;
} // namespace governance
namespace Consensus {
struct Params;
} // namespace Consensus
namespace llmq {
class CInstantSendManager;
class CommitmentPool;
class CQuorumManager;
class CQuorumSnapshotManager;
} // namespace llmq
class CChainstateHelper
{
private:
    llmq::CInstantSendManager& isman;
    const CMasternodeSync& mn_sync;
    const ChainstateManager& m_chainman;

public:
    const chainlock::Chainlocks& m_chainlocks;
    const std::unique_ptr<governance::SuperblockManager> superblocks;
    const std::unique_ptr<CMNPaymentsProcessor> mn_payments;

    CCreditPoolManager& CreditPool() const;
    CMNHFManager& Ehf() const;
    CSpecialTxProcessor& SpecialTx() const;

public:
    CChainstateHelper() = delete;
    CChainstateHelper(const CChainstateHelper&) = delete;
    CChainstateHelper& operator=(const CChainstateHelper&) = delete;
    explicit CChainstateHelper(CDeterministicMNManager& dmnman, const CMasternodeSync& mn_sync,
                               llmq::CInstantSendManager& isman, const ChainstateManager& chainman,
                               const Consensus::Params& consensus_params, const chainlock::Chainlocks& chainlocks);
    ~CChainstateHelper();

    bool IsSuperblockValidationRequired(const CBlockIndex* const pindex);

    /** Passthrough functions to chainlock::Chainlocks */
    bool HasConflictingChainLock(int nHeight, const uint256& blockHash) const;
    bool HasChainLock(int nHeight, const uint256& blockHash) const;
    int32_t GetBestChainLockHeight() const;

    /** Passthrough functions to CCreditPoolManager */
    CCreditPool GetCreditPool(const CBlockIndex* const pindex);

    /** Passthrough functions to CInstantSendManager */
    std::optional<std::pair</*islock_hash=*/uint256, /*txid=*/uint256>> ConflictingISLockIfAny(const CTransaction& tx) const;
    bool IsInstantSendWaitingForTx(const uint256& hash) const;
    bool RemoveConflictingISLockByTx(const CTransaction& tx);

    std::unordered_map<uint8_t, int> GetSignalsStage(const CBlockIndex* const pindexPrev);
};

#endif // BITCOIN_EVO_CHAINHELPER_H
