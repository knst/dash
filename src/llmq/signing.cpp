// Copyright (c) 2018-2024 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <llmq/signing.h>

#include <llmq/commitment.h>
#include <llmq/options.h>
#include <llmq/quorums.h>
#include <llmq/signing_shares.h>

#include <bls/bls_batchverifier.h>
#include <chainparams.h>
#include <cxxtimer.hpp>
#include <dbwrapper.h>
#include <hash.h>
#include <masternode/node.h>
#include <net_processing.h>
#include <netmessagemaker.h>
#include <scheduler.h>
#include <streams.h>
#include <util/irange.h>
#include <util/thread.h>
#include <util/time.h>
#include <util/underlying.h>
#include <validation.h>

#include <algorithm>
#include <unordered_set>

namespace llmq
{
UniValue CRecoveredSig::ToJson() const
{
    UniValue ret(UniValue::VOBJ);
    ret.pushKV("llmqType", ToUnderlying(llmqType));
    ret.pushKV("quorumHash", quorumHash.ToString());
    ret.pushKV("id", id.ToString());
    ret.pushKV("msgHash", msgHash.ToString());
    ret.pushKV("sig", sig.Get().ToString());
    ret.pushKV("hash", sig.Get().GetHash().ToString());
    return ret;
}


uint256 CSigBase::buildSignHash() const
{
    return BuildSignHash(llmqType, quorumHash, id, msgHash);
}

uint256 BuildSignHash(Consensus::LLMQType llmqType, const uint256& quorumHash, const uint256& id, const uint256& msgHash)
{
    CHashWriter h(SER_GETHASH, 0);
    h << llmqType;
    h << quorumHash;
    h << id;
    h << msgHash;
    return h.GetHash();
}

bool IsQuorumActive(Consensus::LLMQType llmqType, const CQuorumManager& qman, const uint256& quorumHash)
{
    // sig shares and recovered sigs are only accepted from recent/active quorums
    // we allow one more active quorum as specified in consensus, as otherwise there is a small window where things could
    // fail while we are on the brink of a new quorum
    const auto& llmq_params_opt = Params().GetLLMQ(llmqType);
    assert(llmq_params_opt.has_value());
    auto quorums = qman.ScanQuorums(llmqType, llmq_params_opt->keepOldConnections);
    return ranges::any_of(quorums, [&quorumHash](const auto& q){ return q->qc->quorumHash == quorumHash; });
}

} // namespace llmq
