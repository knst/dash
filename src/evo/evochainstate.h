// Copyright (c) 2026 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_EVO_EVOCHAINSTATE_H
#define BITCOIN_EVO_EVOCHAINSTATE_H

#include <memory>
#include <optional>

class AbstractEHFManager;
class CCreditPoolManager;
class uint256;
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
class CommitmentPool;
class CQuorumManager;
class CQuorumSnapshotManager;
class MinedCommitmentsStore;
class QuorumResolutionCache;
} // namespace llmq
namespace util {
struct DbWrapperParams;
} // namespace util

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

    //! Open (or create) this chainstate's own EvoDB in `db_params.path`, under
    //! "evodb" for the base chainstate or "evodb_snapshot" for a snapshot
    //! chainstate. A freshly created snapshot EvoDB is seeded with the
    //! snapshot base as its best block so the first connected block passes the
    //! EvoDB/coins consistency check.
    explicit EvoChainState(const util::DbWrapperParams& db_params, std::optional<uint256> snapshot_base,
                           CMasternodeMetaMan& mn_metaman, const ChainstateManager& chainman,
                           const Consensus::Params& consensus_params);
    ~EvoChainState();

    void ConnectLLMQ(llmq::CommitmentPool& cpool, const llmq::CQuorumManager& qman,
                     const chainlock::Chainlocks& chainlocks);
    //! Drop the special-tx processor. Must run before the LLMQ shells it
    //! references are destroyed; the rest of the bundle outlives them.
    void DisconnectLLMQ();

    //! The EHF manager through its versionbits-facing base, so registration
    //! sites need not see the concrete CMNHFManager.
    AbstractEHFManager& EhfManager() const;
};

//! Resolve the active chainstate's Evo state. Defined in validation.cpp so
//! consumers of chain-derived managers need not include validation.h.
EvoChainState& ActiveEvoChainState(const ChainstateManager& chainman);

#endif // BITCOIN_EVO_EVOCHAINSTATE_H
