// Copyright (c) 2026 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_EVO_EVOCHAINSTATE_H
#define BITCOIN_EVO_EVOCHAINSTATE_H

#include <memory>

class CCreditPoolManager;
class CDeterministicMNManager;
class CEvoDB;
class ChainstateManager;
class CMasternodeMetaMan;
class CMNHFManager;
class CSpecialTxProcessor;
namespace chainlock {
class Chainlocks;
} // namespace chainlock
namespace Consensus {
struct Params;
} // namespace Consensus
namespace llmq {
class CQuorumBlockProcessor;
class CQuorumManager;
class CQuorumSnapshotManager;
class MinedCommitmentsStore;
class QuorumResolutionCache;
} // namespace llmq

/**
 * Chain-derived Evo state of one chainstate: the EvoDB it is staged in and
 * every manager whose contents are a function of that chainstate's chain.
 * Node-global services (P2P intake, worker threads, key material) stay
 * outside; they resolve a bundle per call instead of owning one.
 */
class EvoChainState
{
private:
    std::unique_ptr<CEvoDB> m_evodb_owned;
    const ChainstateManager& m_chainman;
    const Consensus::Params& m_consensus_params;

public:
    CEvoDB& evodb;
    const std::unique_ptr<CDeterministicMNManager> dmnman;
    const std::unique_ptr<CMNHFManager> mnhfman;
    const std::unique_ptr<CCreditPoolManager> cpoolman;
    const std::unique_ptr<llmq::CQuorumSnapshotManager> qsnapman;
    const std::unique_ptr<llmq::MinedCommitmentsStore> commitments;
    const std::unique_ptr<llmq::QuorumResolutionCache> qcache;
    //! Built by ConnectLLMQ once the LLMQ shells exist; dies first on teardown.
    std::unique_ptr<CSpecialTxProcessor> special_tx;

    //! Borrow a node-owned EvoDB. Transitional until the bundle owns its DB.
    explicit EvoChainState(CEvoDB& borrowed_evodb, CMasternodeMetaMan& mn_metaman, const ChainstateManager& chainman,
                           const Consensus::Params& consensus_params);
    ~EvoChainState();

    void ConnectLLMQ(llmq::CQuorumBlockProcessor& qblockman, const llmq::CQuorumManager& qman,
                     const chainlock::Chainlocks& chainlocks);
};

#endif // BITCOIN_EVO_EVOCHAINSTATE_H
