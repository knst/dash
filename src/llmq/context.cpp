// Copyright (c) 2018-2026 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <llmq/context.h>

#include <bls/bls_worker.h>
#include <instantsend/instantsend.h>
#include <llmq/blockprocessor.h>
#include <llmq/quorumcache.h>
#include <llmq/quorumsman.h>
#include <llmq/signing.h>
#include <llmq/snapshot.h>
#include <validation.h>

LLMQContext::LLMQContext(CDeterministicMNManager& dmnman, CEvoDB& evo_db, CSporkManager& sporkman,
                         ChainstateManager& chainman, const util::DbWrapperParams& db_params, int8_t bls_threads,
                         int16_t worker_count, int64_t max_recsigs_age) :
    bls_worker{std::make_shared<CBLSWorker>()},
    qsnapman{std::make_unique<llmq::CQuorumSnapshotManager>(evo_db)},
    commitments{std::make_unique<llmq::MinedCommitmentsStore>(evo_db, chainman)},
    quorum_block_processor{std::make_unique<llmq::CQuorumBlockProcessor>(chainman, dmnman, *qsnapman,
                                                                         *commitments, bls_threads)},
    qcache{std::make_unique<llmq::QuorumResolutionCache>(chainman.GetConsensus())},
    qman{std::make_unique<llmq::CQuorumManager>(*bls_worker, dmnman, evo_db, *commitments, *qsnapman,
                                                *qcache, chainman, db_params)},
    sigman{std::make_unique<llmq::CSigningManager>(*qman, db_params, max_recsigs_age)},
    isman{std::make_unique<llmq::CInstantSendManager>(sporkman, db_params)}
{
    // Have to start it early to let VerifyDB check ChainLock signatures in coinbase
    bls_worker->Start(worker_count);
}

LLMQContext::~LLMQContext()
{
    bls_worker->Stop();
}
