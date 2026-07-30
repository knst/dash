// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/chainstate.h>

#include <chain.h>
#include <coins.h>
#include <chainparamsbase.h>
#include <consensus/params.h>
#include <deploymentstatus.h>
#include <node/blockstorage.h>
#include <node/caches.h>
#include <node/context.h>
#include <sync.h>
#include <threadsafety.h>
#include <tinyformat.h>
#include <txdb.h>
#include <txmempool.h>
#include <uint256.h>
#include <util/fs.h>
#include <util/translation.h>
#include <validation.h>
#include <versionbits.h>

#include <bls/bls.h>
#include <evo/chainhelper.h>
#include <evo/deterministicmns.h>
#include <evo/evochainstate.h>
#include <evo/evodb.h>
#include <evo/mnhftx.h>
#include <gsl/pointers.h>
#include <llmq/context.h>
#include <llmq/quorumsman.h>

#include <atomic>
#include <cassert>
#include <memory>
#include <vector>

namespace node {
ChainstateLoadResult LoadChainstate(ChainstateManager& chainman,
                                                     CMasternodeMetaMan& mn_metaman,
                                                     chainlock::Chainlocks& chainlocks,
                                                     CChainstateHelper& chain_helper,
                                                     LLMQContext& llmq_ctx,
                                                     const fs::path& data_dir,
                                                     const CacheSizes& cache_sizes,
                                                     const ChainstateLoadOptions& options)
{
    auto is_coinsview_empty = [&](Chainstate* chainstate) EXCLUSIVE_LOCKS_REQUIRED(::cs_main) {
        return options.reindex || options.reindex_chainstate || chainstate->CoinsTip().GetBestBlock().IsNull();
    };

    LOCK(cs_main);
    chainman.m_total_coinstip_cache = cache_sizes.coins;
    chainman.m_total_coinsdb_cache = cache_sizes.coins_db;

    // Load the fully validated chainstate.
    chainman.InitializeChainstate(options.mempool, chain_helper);

    // Load a chain created from a UTXO snapshot, if any exist.
    chainman.DetectSnapshotChainstate(options.mempool);

    if (!chainman.SnapshotBlockhash() && fs::exists(data_dir / "evodb_snapshot")) {
        // Leftover of an interrupted snapshot activation; the matching coins
        // snapshot is gone, so the Evo side must go too.
        LogPrintf("Removing stale evodb_snapshot directory (%s)\n", fs::PathToString(data_dir / "evodb_snapshot"));
        fs::remove_all(data_dir / "evodb_snapshot");
    }

    auto& pblocktree{chainman.m_blockman.m_block_tree_db};
    // new CBlockTreeDB tries to delete the existing file, which
    // fails if it's still open from the previous loop. Close it first:
    pblocktree.reset();
    pblocktree.reset(new CBlockTreeDB(cache_sizes.block_tree_db, options.block_tree_db_in_memory, options.reindex));

    DashChainstateSetup(chainman, mn_metaman, chainlocks, llmq_ctx, options.mempool, data_dir,
                        options.dash_dbs_in_memory,
                        /*dash_dbs_wipe=*/options.reindex || options.reindex_chainstate);

    if (options.reindex) {
        pblocktree->WriteReindexing(true);
        //If we're reindexing in prune mode, wipe away unusable block files and all undo data files
        if (options.prune) {
            CleanupBlockRevFiles();
        }
    }

    if (options.check_interrupt && options.check_interrupt()) return {ChainstateLoadStatus::INTERRUPTED, {}};

    // LoadBlockIndex will load m_have_pruned if we've ever removed a
    // block file from disk.
    // Note that it also sets fReindex global based on the disk flag!
    // From here on, fReindex and options.reindex values may be different!
    if (!chainman.LoadBlockIndex()) {
        if (options.check_interrupt && options.check_interrupt()) return {ChainstateLoadStatus::INTERRUPTED, {}};
        return {ChainstateLoadStatus::FAILURE, _("Error loading block database")};
    }

    if (!chainman.BlockIndex().empty() &&
            !chainman.m_blockman.LookupBlockIndex(chainman.GetConsensus().hashGenesisBlock)) {
        return {ChainstateLoadStatus::FAILURE_INCOMPATIBLE_DB, _("Incorrect or no genesis block found. Wrong datadir for network?")};
    }

    if (!chainman.GetConsensus().hashDevnetGenesisBlock.IsNull() && !chainman.BlockIndex().empty() &&
            !chainman.m_blockman.LookupBlockIndex(chainman.GetConsensus().hashDevnetGenesisBlock)) {
        return {ChainstateLoadStatus::FAILURE_INCOMPATIBLE_DB, _("Incorrect or no devnet genesis block found. Wrong datadir for devnet specified?")};
    }

    // Check for changed -prune state.  What we are concerned about is a user who has pruned blocks
    // in the past, but is now trying to run unpruned.
    if (chainman.m_blockman.m_have_pruned && !options.prune) {
        return {ChainstateLoadStatus::FAILURE, _("You need to rebuild the database using -reindex to go back to unpruned mode.  This will redownload the entire blockchain")};
    }

    // At this point blocktree args are consistent with what's on disk.
    // If we're not mid-reindex (based on disk + args), add a genesis block on disk
    // (otherwise we use the one already on disk).
    // This is called again in ThreadImport after the reindex completes.
    if (!fReindex && !chainman.ActiveChainstate().LoadGenesisBlock()) {
        return {ChainstateLoadStatus::FAILURE, _("Error initializing block database")};
    }

    // Conservative value which is arbitrarily chosen, as it will ultimately be changed
    // by a call to `chainman.MaybeRebalanceCaches()`. We just need to make sure
    // that the sum of the two caches (40%) does not exceed the allowable amount
    // during this temporary initialization state.
    double init_cache_fraction = 0.2;

    // At this point we're either in reindex or we've loaded a useful
    // block tree into BlockIndex()!

    for (Chainstate* chainstate : chainman.GetAll()) {
        LogPrintf("Initializing chainstate %s\n", chainstate->ToString());

        chainstate->InitCoinsDB(
            /*cache_size_bytes=*/chainman.m_total_coinsdb_cache * init_cache_fraction,
            /*in_memory=*/options.coins_db_in_memory,
            /*should_wipe=*/options.reindex || options.reindex_chainstate);

        if (options.coins_error_cb) {
            chainstate->CoinsErrorCatcher().AddReadErrCallback(options.coins_error_cb);
        }

        // Refuse to load unsupported database format.
        // This is a no-op if we cleared the coinsviewdb with -reindex or -reindex-chainstate
        if (chainstate->CoinsDB().NeedsUpgrade()) {
            return {ChainstateLoadStatus::FAILURE_INCOMPATIBLE_DB, _("Unsupported chainstate database format found. "
                                                                     "Please restart with -reindex-chainstate. This will "
                                                                     "rebuild the chainstate database.")};
        }

        // ReplayBlocks is a no-op if we cleared the coinsviewdb with -reindex or -reindex-chainstate
        if (!chainstate->ReplayBlocks()) {
            return {ChainstateLoadStatus::FAILURE, _("Unable to replay blocks. You will need to rebuild the database using -reindex-chainstate.")};
        }

        // The on-disk coinsdb is now in a good state, create the cache
        chainstate->InitCoinsCache(chainman.m_total_coinstip_cache * init_cache_fraction);
        assert(chainstate->CanFlushToDisk());

        // flush evodb
        if (!chainstate->Evo().evodb.CommitRootTransaction()) {
            return {ChainstateLoadStatus::FAILURE, _("Failed to commit Evo database")};
        }

        if (!is_coinsview_empty(chainstate)) {
            // LoadChainTip initializes the chain based on CoinsTip()'s best block
            if (!chainstate->LoadChainTip()) {
                return {ChainstateLoadStatus::FAILURE, _("Error initializing block database")};
            }
            assert(chainstate->m_chain.Tip() != nullptr);
        }
    }

    if (!chainman.ActiveChainstate().Evo().mnhfman->ForceSignalDBUpdate()) {
        return {ChainstateLoadStatus::FAILURE, _("Error upgrading evo database for EHF")};
    }

    // Check if nVersion-first migration is needed and perform it
    if (CDeterministicMNManager& dmnman = *chainman.ActiveChainstate().Evo().dmnman;
        dmnman.IsMigrationRequired() && !dmnman.MigrateLegacyDiffs(chainman.ActiveChainstate().m_chain.Tip())) {
        return {ChainstateLoadStatus::FAILURE, _("Failed to upgrade Evo database")};
    }

    // Now that chainstates are loaded and we're able to flush to
    // disk, rebalance the coins caches to desired levels based
    // on the condition of each chainstate.
    chainman.MaybeRebalanceCaches();

    return {ChainstateLoadStatus::SUCCESS, {}};
}

void DashChainstateSetup(ChainstateManager& chainman,
                         CMasternodeMetaMan& mn_metaman,
                         chainlock::Chainlocks& chainlocks,
                         LLMQContext& llmq_ctx,
                         CTxMemPool* mempool,
                         const fs::path& data_dir,
                         bool dash_dbs_in_memory,
                         bool dash_dbs_wipe)
{
    chainman.m_make_evo_chainstate = [&chainman, &mn_metaman, &chainlocks,
                                      cpool = llmq_ctx.commitment_pool.get(),
                                      qman = llmq_ctx.qman.get(), data_dir,
                                      dash_dbs_in_memory](Chainstate& chainstate, bool wipe) {
        auto evo_state = std::make_unique<EvoChainState>(
            util::DbWrapperParams{.path = data_dir, .memory = dash_dbs_in_memory, .wipe = wipe},
            chainstate.m_from_snapshot_blockhash, mn_metaman, chainman, chainman.GetConsensus());
        evo_state->ConnectLLMQ(*cpool, *qman, chainlocks);
        return evo_state;
    };

    {
        LOCK(::cs_main);
        for (Chainstate* chainstate : chainman.GetAll()) {
            chainstate->ResetEvoChainState();
            chainstate->InitEvoChainState(chainman.m_make_evo_chainstate(*chainstate, dash_dbs_wipe));
            llmq_ctx.qman->MigrateOldQuorumDB(chainstate->Evo().evodb);
        }
    }

    EvoChainState& active_evo = WITH_LOCK(::cs_main, return chainman.ActiveChainstate().Evo());
    AbstractEHFManager::RegisterInstance(active_evo.mnhfman.get());
    if (mempool) {
        // Disconnect first so re-running setup rebinds to the new active bundle.
        mempool->DisconnectManagers();
        mempool->ConnectManagers(active_evo.dmnman.get(), llmq_ctx.isman.get());
    }
}

void BindActiveEvoViews(ChainstateManager& chainman, NodeContext& node)
{
    LOCK(::cs_main);
    EvoChainState& evo = chainman.ActiveChainstate().Evo();
    node.dmnman = evo.dmnman.get();
    node.qsnapman = evo.qsnapman.get();
    node.commitments = evo.commitments.get();
    node.evodb = &evo.evodb;
}

void ClearEvoViews(NodeContext& node)
{
    node.dmnman = nullptr;
    node.qsnapman = nullptr;
    node.commitments = nullptr;
    node.evodb = nullptr;
}

void DashChainstateSetupClose(ChainstateManager& chainman,
                              std::unique_ptr<CChainstateHelper>& chain_helper,
                              std::unique_ptr<LLMQContext>& llmq_ctx,
                              CTxMemPool* mempool)

{
    chain_helper.reset();
    if (mempool) {
        mempool->DisconnectManagers();
    }
    // The special-tx processor references the LLMQ shells and must go first;
    // the shells' worker threads reference the rest of the bundle, so the
    // bundle itself is torn down only after the shells are gone.
    {
        LOCK(::cs_main);
        for (Chainstate* chainstate : chainman.GetAll()) {
            chainstate->DisconnectEvoLLMQ();
        }
    }
    chainman.m_make_evo_chainstate = nullptr;
    llmq_ctx.reset();
    {
        LOCK(::cs_main);
        for (Chainstate* chainstate : chainman.GetAll()) {
            chainstate->ResetEvoChainState();
        }
    }
    AbstractEHFManager::RegisterInstance(nullptr);
}

void DashChainstateSetupClose(NodeContext& node)
{
    if (node.chainman) {
        DashChainstateSetupClose(*node.chainman, node.chain_helper, node.llmq_ctx, node.mempool.get());
    }
    ClearEvoViews(node);
}

ChainstateLoadResult VerifyLoadedChainstate(ChainstateManager& chainman,
                                            const ChainstateLoadOptions& options)
{
    auto is_coinsview_empty = [&](Chainstate* chainstate) EXCLUSIVE_LOCKS_REQUIRED(::cs_main) {
        return options.reindex || options.reindex_chainstate || chainstate->CoinsTip().GetBestBlock().IsNull();
    };

    LOCK(cs_main);

    for (Chainstate* chainstate : chainman.GetAll()) {
        if (!is_coinsview_empty(chainstate)) {
            const CBlockIndex* tip = chainstate->m_chain.Tip();
            if (tip && tip->nTime > GetTime() + MAX_FUTURE_BLOCK_TIME) {
                return {ChainstateLoadStatus::FAILURE, _("The block database contains a block which appears to be from the future. "
                                                         "This may be due to your computer's date and time being set incorrectly. "
                                                         "Only rebuild the block database if you are sure that your computer's date and time are correct")};
            }
            const bool v19active{DeploymentActiveAfter(tip, chainman, Consensus::DEPLOYMENT_V19)};
            if (v19active) {
                bls::bls_legacy_scheme.store(false);
                if (options.notify_bls_state) options.notify_bls_state(bls::bls_legacy_scheme.load());
            }

            if (!CVerifyDB().VerifyDB(
                    *chainstate, chainman.GetConsensus(), chainstate->CoinsDB(),
                    options.check_level,
                    options.check_blocks)) {
                return {ChainstateLoadStatus::FAILURE, _("Corrupted block database detected")};
            }

            // VerifyDB() disconnects blocks which might result in us switching back to legacy.
            // Make sure we use the right scheme.
            if (v19active && bls::bls_legacy_scheme.load()) {
                bls::bls_legacy_scheme.store(false);
                if (options.notify_bls_state) options.notify_bls_state(bls::bls_legacy_scheme.load());
            }

            if (options.check_level >= 3) {
                chainstate->ResetBlockFailureFlags(nullptr);
            }

        } else {
            // A freshly seeded snapshot EvoDB legitimately has content before
            // any block is connected; only the base chainstate check applies.
            if (!chainstate->m_from_snapshot_blockhash && !chainstate->Evo().evodb.IsEmpty()) {
                // EvoDB processed some blocks earlier but we have no blocks anymore, something is wrong
                return {ChainstateLoadStatus::FAILURE, _("Error initializing block database")};
            }
        }
    }

    return {ChainstateLoadStatus::SUCCESS, {}};
}
} // namespace node
