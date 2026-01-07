// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// The bitcoin-chainstate executable serves to surface the dependencies required
// by a program wishing to use Bitcoin Core's consensus engine as it is right
// now.
//
// DEVELOPER NOTE: Since this is a "demo-only", experimental, etc. executable,
//                 it may diverge from Bitcoin Core's coding style.
//
// It is part of the libbitcoinkernel project.

#include <chainparams.h>
#include <consensus/validation.h>
#include <core_io.h>
#include <init/common.h>
#include <node/blockstorage.h>
#include <node/chainstate.h>
#include <scheduler.h>
#include <script/sigcache.h>
#include <util/system.h>
#include <util/thread.h>
#include <validation.h>
#include <validationinterface.h>


// TODO: review & probably remove
#include <test/util/net.h>
#include <governance/governance.h>
#include <evo/chainhelper.h>
#include <evo/evodb.h>
#include <netfulfilledman.h>
#include <masternode/node.h>
#include <masternode/meta.h>
#include <masternode/sync.h>
#include <spork.h>

#include <filesystem>
#include <functional>
#include <iosfwd>

const std::function<std::string(const char*)> G_TRANSLATION_FUN = nullptr;

int main(int argc, char* argv[])
{
    // SETUP: Argument parsing and handling
    if (argc != 2) {
        std::cerr
            << "Usage: " << argv[0] << " DATADIR" << std::endl
            << "Display DATADIR information, and process hex-encoded blocks on standard input." << std::endl
            << std::endl
            << "IMPORTANT: THIS EXECUTABLE IS EXPERIMENTAL, FOR TESTING ONLY, AND EXPECTED TO" << std::endl
            << "           BREAK IN FUTURE VERSIONS. DO NOT USE ON YOUR ACTUAL DATADIR." << std::endl;
        return 1;
    }
    std::filesystem::path abs_datadir = std::filesystem::absolute(argv[1]);
    std::filesystem::create_directories(abs_datadir);
    gArgs.ForceSetArg("-datadir", abs_datadir.string());


    // SETUP: Misc Globals
    SelectParams(CBaseChainParams::MAIN);
    const CChainParams& chainparams = Params();

    init::SetGlobals(); // ECC_Start, etc.

    // Necessary for CheckInputScripts (eventually called by ProcessNewBlock),
    // which will try the script cache first and fall back to actually
    // performing the check with the signature cache.
    InitSignatureCache();
    InitScriptExecutionCache();


    // SETUP: Scheduling and Background Signals
    CScheduler scheduler{};
    // Start the lightweight task scheduler thread
    scheduler.m_service_thread = std::thread(util::TraceThread, "scheduler", [&] { scheduler.serviceQueue(); });

    // Gather some entropy once per minute.
    scheduler.scheduleEvery(RandAddPeriodic, std::chrono::minutes{1});

    GetMainSignals().RegisterBackgroundSignalScheduler(scheduler);


    // SETUP: Chainstate
    ChainstateManager chainman;

    /*
    auto maybe_load_error = LoadChainstate(fReindex.load(),
                                           *Assert(m_node.chainman.get()),
                                           *assert(m_node.govman.get()),
                                           *assert(m_node.mn_metaman.get()),
                                           *assert(m_node.mn_sync.get()),
                                           *assert(m_node.sporkman.get()),
                                           m_node.mn_activeman,
                                           m_node.chain_helper,
                                           m_node.cpoolman,
                                           m_node.dmnman,
                                           m_node.evodb,
                                           m_node.mnhf_manager,
                                           m_node.llmq_ctx,
                                           Assert(m_node.mempool.get()),
                                           fPruneMode,
                                           m_args.GetBoolArg("-addressindex", DEFAULT_ADDRESSINDEX),
                                           !m_args.GetBoolArg("-disablegovernance", !DEFAULT_GOVERNANCE_ENABLE),
                                           m_args.GetBoolArg("-spentindex", DEFAULT_SPENTINDEX),
                                           m_args.GetBoolArg("-timestampindex", DEFAULT_TIMESTAMPINDEX),
                                           m_args.GetBoolArg("-txindex", DEFAULT_TXINDEX),
                                           chainparams.GetConsensus(),
                                           chainparams.NetworkIDString(),
                                           m_args.GetBoolArg("-reindex-chainstate", false),
                                           m_cache_sizes.block_tree_db,
                                           m_cache_sizes.coins_db,
                                           m_cache_sizes.coins,
                                           block_tree_db_in_memory=true,
                                           coins_db_in_memory=true);
                                           */
    // TODO: remove govman? remove govman in Dash Core if -disablegovernance?
    const auto netgroupman = std::make_unique<NetGroupManager>(/*asmap=*/std::vector<bool>());
    const auto addrman = std::make_unique<AddrMan>(netgroupman,
                                               /*deterministic=*/false,
                                               0);
    const auto connman = std::make_unique<ConnmanTestMsg>(0x1337, 0x1337, *addrman, *netgroupman); // Deterministic randomness for tests.
    const auto evodb = std::make_unique<CEvoDB>(1 << 20, true, true);
    const auto mnhf_manager = std::make_unique<CMNHFManager>(*m_node.evodb);
    const auto cpoolman = std::make_unique<CCreditPoolManager>(*m_node.evodb);
    const auto mn_metaman = std::make_unique<CMasternodeMetaMan>();
    const auto netfulfilledman = std::make_unique<CNetFulfilledRequestManager>();
    const auto sporkman = std::make_unique<CSporkManager>();
    const auto mn_sync = std::make_unique<CMasternodeSync>(connman, netfulfilledman);
    const auto govman = std::make_unique<CGovernanceManager>(*mn_metaman, *netfulfilledman, chainman, /*dmnman=*/nullptr, mn_sync);
    std::unique_ptr<CActiveMasternodeManager> mn_activeman{nullptr};
    const auto chain_helper = std::make_unique<CChainstateHelper>(*cpoolman, *dmnman, *mnhf_manager, govman, *(llmq_ctx->isman), *(llmq_ctx->quorum_block_processor),
                                                       *(llmq_ctx->qsnapman), chainman, consensus_params, mn_sync, sporkman, *(llmq_ctx->clhandler),
                                                       *(llmq_ctx->qman));
    auto rv = node::LoadChainstate(false,
                                   std::ref(chainman),
                                           *Assert(govman.get()), // dash
                                           *Assert(mn_metaman.get()), // dash
                                           *Assert(mn_sync.get()), // dash
                                           *Assert(sporkman.get()), // dash
                                           mn_activeman, //m_node.mn_activeman, // dash
                                           chain_helper, //m_node.chain_helper, // dash
                                           cpoolman, // m_node.cpoolman, // dash
                                           nullptr, // m_node.dmnman, // dash
                                           evodb, //m_node.evodb, // dash
                                           mnhf_manager, //m_node.mnhf_manager, // dash
                                           nullptr, // m_node.llmq_ctx, // dash
                                   nullptr, // mempool ?
                                   false,
                                           false, //m_args.GetBoolArg("-addressindex", DEFAULT_ADDRESSINDEX),
                                           true, //!m_args.GetBoolArg("-disablegovernance", !DEFAULT_GOVERNANCE_ENABLE),
                                           false, //m_args.GetBoolArg("-spentindex", DEFAULT_SPENTINDEX),
                                           false, // m_args.GetBoolArg("-timestampindex", DEFAULT_TIMESTAMPINDEX),
                                           false, // m_args.GetBoolArg("-txindex", DEFAULT_TXINDEX),
                                   chainparams.GetConsensus(),
                                   chainparams.NetworkIDString(), // dash
                                   false,
                                   2 << 20,
                                   2 << 22,
                                   (450 << 20) - (2 << 20) - (2 << 22),
                                   false,
                                   false,
                                   []() { return false; });
    if (rv.has_value()) {
        std::cerr << "Failed to load Chain state from your datadir." << std::endl;
        goto epilogue;
    } else {
        auto maybe_verify_error = node::VerifyLoadedChainstate(std::ref(chainman),
                                                               false,
                                                               false,
                                                               chainparams.GetConsensus(),
                                                               DEFAULT_CHECKBLOCKS,
                                                               DEFAULT_CHECKLEVEL,
                                                               /*get_unix_time_seconds=*/static_cast<int64_t (*)()>(GetTime));
        if (maybe_verify_error.has_value()) {
            std::cerr << "Failed to verify loaded Chain state from your datadir." << std::endl;
            goto epilogue;
        }
    }

    for (CChainState* chainstate : WITH_LOCK(::cs_main, return chainman.GetAll())) {
        BlockValidationState state;
        if (!chainstate->ActivateBestChain(state, nullptr)) {
            std::cerr << "Failed to connect best block (" << state.ToString() << ")" << std::endl;
            goto epilogue;
        }
    }

    // Main program logic starts here
    std::cout
        << "Hello! I'm going to print out some information about your datadir." << std::endl
        << "\t" << "Path: " << gArgs.GetDataDirNet() << std::endl
        << "\t" << "Reindexing: " << std::boolalpha << node::fReindex.load() << std::noboolalpha << std::endl
        << "\t" << "Snapshot Active: " << std::boolalpha << chainman.IsSnapshotActive() << std::noboolalpha << std::endl
        << "\t" << "Active Height: " << chainman.ActiveHeight() << std::endl
        << "\t" << "Active IBD: " << std::boolalpha << chainman.ActiveChainstate().IsInitialBlockDownload() << std::noboolalpha << std::endl;
    {
        CBlockIndex* tip = chainman.ActiveTip();
        if (tip) {
            std::cout << "\t" << tip->ToString() << std::endl;
        }
    }

    for (std::string line; std::getline(std::cin, line);) {
        if (line.empty()) {
            std::cerr << "Empty line found" << std::endl;
            break;
        }

        std::shared_ptr<CBlock> blockptr = std::make_shared<CBlock>();
        CBlock& block = *blockptr;

        if (!DecodeHexBlk(block, line)) {
            std::cerr << "Block decode failed" << std::endl;
            break;
        }

        if (block.vtx.empty() || !block.vtx[0]->IsCoinBase()) {
            std::cerr << "Block does not start with a coinbase" << std::endl;
            break;
        }

        uint256 hash = block.GetHash();
        {
            LOCK(cs_main);
            const CBlockIndex* pindex = chainman.m_blockman.LookupBlockIndex(hash);
            if (pindex) {
                if (pindex->IsValid(BLOCK_VALID_SCRIPTS)) {
                    std::cerr << "duplicate" << std::endl;
                    break;
                }
                if (pindex->nStatus & BLOCK_FAILED_MASK) {
                    std::cerr << "duplicate-invalid" << std::endl;
                    break;
                }
            }
        }

        {
            LOCK(cs_main);
            const CBlockIndex* pindex = chainman.m_blockman.LookupBlockIndex(block.hashPrevBlock);
            if (pindex) {
                UpdateUncommittedBlockStructures(block, pindex, chainparams.GetConsensus());
            }
        }

        // Adapted from rpc/mining.cpp
        class submitblock_StateCatcher final : public CValidationInterface
        {
        public:
            uint256 hash;
            bool found;
            BlockValidationState state;

            explicit submitblock_StateCatcher(const uint256& hashIn) : hash(hashIn), found(false), state() {}

        protected:
            void BlockChecked(const CBlock& block, const BlockValidationState& stateIn) override
            {
                if (block.GetHash() != hash)
                    return;
                found = true;
                state = stateIn;
            }
        };

        bool new_block;
        auto sc = std::make_shared<submitblock_StateCatcher>(block.GetHash());
        RegisterSharedValidationInterface(sc);
        bool accepted = chainman.ProcessNewBlock(chainparams, blockptr, /* force_processing */ true, /* new_block */ &new_block);
        UnregisterSharedValidationInterface(sc);
        if (!new_block && accepted) {
            std::cerr << "duplicate" << std::endl;
            break;
        }
        if (!sc->found) {
            std::cerr << "inconclusive" << std::endl;
            break;
        }
        std::cout << sc->state.ToString() << std::endl;
        switch (sc->state.GetResult()) {
        case BlockValidationResult::BLOCK_RESULT_UNSET:
            std::cerr << "initial value. Block has not yet been rejected" << std::endl;
            break;
        case BlockValidationResult::BLOCK_CONSENSUS:
            std::cerr << "invalid by consensus rules (excluding any below reasons)" << std::endl;
            break;
        case BlockValidationResult::BLOCK_RECENT_CONSENSUS_CHANGE:
            std::cerr << "Invalid by a change to consensus rules more recent than SegWit." << std::endl;
            break;
        case BlockValidationResult::BLOCK_CACHED_INVALID:
            std::cerr << "this block was cached as being invalid and we didn't store the reason why" << std::endl;
            break;
        case BlockValidationResult::BLOCK_INVALID_HEADER:
            std::cerr << "invalid proof of work or time too old" << std::endl;
            break;
        case BlockValidationResult::BLOCK_MUTATED:
            std::cerr << "the block's data didn't match the data committed to by the PoW" << std::endl;
            break;
        case BlockValidationResult::BLOCK_MISSING_PREV:
            std::cerr << "We don't have the previous block the checked one is built on" << std::endl;
            break;
        case BlockValidationResult::BLOCK_INVALID_PREV:
            std::cerr << "A block this one builds on is invalid" << std::endl;
            break;
        case BlockValidationResult::BLOCK_TIME_FUTURE:
            std::cerr << "block timestamp was > 2 hours in the future (or our clock is bad)" << std::endl;
            break;
        case BlockValidationResult::BLOCK_CHECKPOINT:
            std::cerr << "the block failed to meet one of our checkpoints" << std::endl;
            break;
        }
    }

epilogue:
    // Without this precise shutdown sequence, there will be a lot of nullptr
    // dereferencing and UB.
    scheduler.stop();
    if (chainman.m_load_block.joinable()) chainman.m_load_block.join();
    StopScriptCheckWorkerThreads();

    GetMainSignals().FlushBackgroundCallbacks();
    {
        LOCK(cs_main);
        for (CChainState* chainstate : chainman.GetAll()) {
            if (chainstate->CanFlushToDisk()) {
                chainstate->ForceFlushStateToDisk();
                chainstate->ResetCoinsViews();
            }
        }
    }
    GetMainSignals().UnregisterBackgroundSignalScheduler();

    UnloadBlockIndex(nullptr, chainman);

    init::UnsetGlobals();
}
