// Copyright (c) 2018-2024 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_LLMQ_SIGNING_H
#define BITCOIN_LLMQ_SIGNING_H

#include <bls/bls.h>
#include <consensus/params.h>
#include <gsl/pointers.h>
#include <protocol.h>
#include <random.h>
#include <saltedhasher.h>
#include <sync.h>
#include <threadinterrupt.h>
#include <univalue.h>
#include <unordered_lru_cache.h>

#include <unordered_map>

class CActiveMasternodeManager;
class CChainState;
class CConnman;
class CDataStream;
class CDBBatch;
class CDBWrapper;
class CInv;
class CNode;
class PeerManager;

using NodeId = int64_t;

namespace llmq
{
class CQuorum;
using CQuorumCPtr = std::shared_ptr<const CQuorum>;
class CQuorumManager;
class CSigSharesManager;

// Keep recovered signatures for a week. This is a "-maxrecsigsage" option default.
static constexpr int64_t DEFAULT_MAX_RECOVERED_SIGS_AGE{60 * 60 * 24 * 7};

class CSigBase
{
protected:
    Consensus::LLMQType llmqType{Consensus::LLMQType::LLMQ_NONE};
    uint256 quorumHash;
    uint256 id;
    uint256 msgHash;

    CSigBase(Consensus::LLMQType llmqType, const uint256& quorumHash, const uint256& id, const uint256& msgHash)
            : llmqType(llmqType), quorumHash(quorumHash), id(id), msgHash(msgHash) {};
    CSigBase() = default;

public:
    [[nodiscard]] constexpr auto getLlmqType() const {
        return llmqType;
    }

    [[nodiscard]] constexpr auto getQuorumHash() const -> const uint256& {
        return quorumHash;
    }

    [[nodiscard]] constexpr auto getId() const -> const uint256& {
        return id;
    }

    [[nodiscard]] constexpr auto getMsgHash() const -> const uint256& {
        return msgHash;
    }

    [[nodiscard]] uint256 buildSignHash() const;
};

class CRecoveredSig : virtual public CSigBase
{
public:
    const CBLSLazySignature sig;

    CRecoveredSig() = default;

    CRecoveredSig(Consensus::LLMQType _llmqType, const uint256& _quorumHash, const uint256& _id, const uint256& _msgHash, const CBLSLazySignature& _sig) :
                  CSigBase(_llmqType, _quorumHash, _id, _msgHash), sig(_sig) {UpdateHash();};
    CRecoveredSig(Consensus::LLMQType _llmqType, const uint256& _quorumHash, const uint256& _id, const uint256& _msgHash, const CBLSSignature& _sig) :
                  CSigBase(_llmqType, _quorumHash, _id, _msgHash) {const_cast<CBLSLazySignature&>(sig).Set(_sig, bls::bls_legacy_scheme.load()); UpdateHash();};

private:
    // only in-memory
    uint256 hash;

    void UpdateHash()
    {
        hash = ::SerializeHash(*this);
    }

public:
    SERIALIZE_METHODS(CRecoveredSig, obj)
    {
        READWRITE(const_cast<Consensus::LLMQType&>(obj.llmqType), const_cast<uint256&>(obj.quorumHash), const_cast<uint256&>(obj.id),
                  const_cast<uint256&>(obj.msgHash), const_cast<CBLSLazySignature&>(obj.sig));
        SER_READ(obj, obj.UpdateHash());
    }

    const uint256& GetHash() const
    {
        assert(!hash.IsNull());
        return hash;
    }

    UniValue ToJson() const;
};


class CRecoveredSigsListener
{
public:
    virtual ~CRecoveredSigsListener() = default;

    [[nodiscard]] virtual MessageProcessingResult HandleNewRecoveredSig(const CRecoveredSig& recoveredSig) = 0;
};

template<typename NodesContainer, typename Continue, typename Callback>
void IterateNodesRandom(NodesContainer& nodeStates, Continue&& cont, Callback&& callback, FastRandomContext& rnd)
{
    std::vector<typename NodesContainer::iterator> rndNodes;
    rndNodes.reserve(nodeStates.size());
    for (auto it = nodeStates.begin(); it != nodeStates.end(); ++it) {
        rndNodes.emplace_back(it);
    }
    if (rndNodes.empty()) {
        return;
    }
    Shuffle(rndNodes.begin(), rndNodes.end(), rnd);

    size_t idx = 0;
    while (!rndNodes.empty() && cont()) {
        auto nodeId = rndNodes[idx]->first;
        auto& ns = rndNodes[idx]->second;

        if (callback(nodeId, ns)) {
            idx = (idx + 1) % rndNodes.size();
        } else {
            rndNodes.erase(rndNodes.begin() + idx);
            if (rndNodes.empty()) {
                break;
            }
            idx %= rndNodes.size();
        }
    }
}

uint256 BuildSignHash(Consensus::LLMQType llmqType, const uint256& quorumHash, const uint256& id, const uint256& msgHash);

bool IsQuorumActive(Consensus::LLMQType llmqType, const CQuorumManager& qman, const uint256& quorumHash);

} // namespace llmq

#endif // BITCOIN_LLMQ_SIGNING_H
