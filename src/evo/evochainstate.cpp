// Copyright (c) 2026 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <evo/evochainstate.h>

#include <evo/creditpool.h>
#include <evo/deterministicmns.h>
#include <evo/evodb.h>
#include <evo/mnhftx.h>
#include <evo/specialtxman.h>
#include <llmq/quorumcache.h>
#include <llmq/snapshot.h>
#include <node/utxo_snapshot.h>
#include <uint256.h>

EvoChainState::EvoChainState(const util::DbWrapperParams& db_params, std::optional<uint256> snapshot_base,
                             CMasternodeMetaMan& mn_metaman, const ChainstateManager& chainman,
                             const Consensus::Params& consensus_params) :
    m_evodb_owned{std::make_unique<CEvoDB>(
        db_params, snapshot_base ? "evodb" + std::string{node::SNAPSHOT_CHAINSTATE_SUFFIX} : "evodb")},
    m_chainman{chainman},
    m_consensus_params{consensus_params},
    evodb{*m_evodb_owned},
    dmnman{std::make_unique<CDeterministicMNManager>(evodb, mn_metaman)},
    mnhfman{std::make_unique<CMNHFManager>(evodb, chainman)},
    cpoolman{std::make_unique<CCreditPoolManager>(evodb, chainman)},
    qsnapman{std::make_unique<llmq::CQuorumSnapshotManager>(evodb)},
    commitments{std::make_unique<llmq::MinedCommitmentsStore>(evodb, chainman)},
    qcache{std::make_unique<llmq::QuorumResolutionCache>(consensus_params)}
{
    if (snapshot_base && evodb.IsEmpty()) {
        auto db_tx = evodb.BeginTransaction();
        evodb.WriteBestBlock(*snapshot_base);
        db_tx->Commit();
        bool committed = evodb.CommitRootTransaction();
        assert(committed);
    }
}

EvoChainState::~EvoChainState() = default;

void EvoChainState::ConnectLLMQ(llmq::CQuorumBlockProcessor& qblockman, const llmq::CQuorumManager& qman,
                                const chainlock::Chainlocks& chainlocks)
{
    special_tx = std::make_unique<CSpecialTxProcessor>(*cpoolman, *dmnman, *mnhfman, qblockman, *commitments, *qsnapman,
                                                       m_chainman, m_consensus_params, chainlocks, qman);
}

void EvoChainState::DisconnectLLMQ()
{
    special_tx.reset();
}
