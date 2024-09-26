// Copyright (c) 2024 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_LLMQ_SIGNING_MANAGER_H
#define BITCOIN_LLMQ_SIGNING_MANAGER_H

#include <llmq/signing.h>

class CRecoveredSigsDb
{
private:
    std::unique_ptr<CDBWrapper> db{nullptr};

    mutable Mutex cs_cache;
    mutable unordered_lru_cache<std::pair<Consensus::LLMQType, uint256>, bool, StaticSaltedHasher, 30000> hasSigForIdCache GUARDED_BY(cs_cache);
    mutable unordered_lru_cache<uint256, bool, StaticSaltedHasher, 30000> hasSigForSessionCache GUARDED_BY(cs_cache);
    mutable unordered_lru_cache<uint256, bool, StaticSaltedHasher, 30000> hasSigForHashCache GUARDED_BY(cs_cache);

public:
    explicit CRecoveredSigsDb(bool fMemory, bool fWipe);
    ~CRecoveredSigsDb();

    bool HasRecoveredSig(Consensus::LLMQType llmqType, const uint256& id, const uint256& msgHash) const;
    bool HasRecoveredSigForId(Consensus::LLMQType llmqType, const uint256& id) const;
    bool HasRecoveredSigForSession(const uint256& signHash) const;
    bool HasRecoveredSigForHash(const uint256& hash) const;
    bool GetRecoveredSigByHash(const uint256& hash, CRecoveredSig& ret) const;
    bool GetRecoveredSigById(Consensus::LLMQType llmqType, const uint256& id, CRecoveredSig& ret) const;
    void WriteRecoveredSig(const CRecoveredSig& recSig);
    void TruncateRecoveredSig(Consensus::LLMQType llmqType, const uint256& id);

    void CleanupOldRecoveredSigs(int64_t maxAge);

    // votes are removed when the recovered sig is written to the db
    bool HasVotedOnId(Consensus::LLMQType llmqType, const uint256& id) const;
    bool GetVoteForId(Consensus::LLMQType llmqType, const uint256& id, uint256& msgHashRet) const;
    void WriteVoteForId(Consensus::LLMQType llmqType, const uint256& id, const uint256& msgHash);

    void CleanupOldVotes(int64_t maxAge);

private:
    void MigrateRecoveredSigs();

    bool ReadRecoveredSig(Consensus::LLMQType llmqType, const uint256& id, CRecoveredSig& ret) const;
    void RemoveRecoveredSig(CDBBatch& batch, Consensus::LLMQType llmqType, const uint256& id, bool deleteHashKey, bool deleteTimeKey);
};

class SigningManagerImpl : public SigningManager
{
private:

    CRecoveredSigsDb db;
    CConnman& connman;
    const CActiveMasternodeManager* const m_mn_activeman;
    const CChainState& m_chainstate;
    const CQuorumManager& qman;
    const std::unique_ptr<PeerManager>& m_peerman;

    mutable Mutex cs_pending;
    // Incoming and not verified yet
    std::unordered_map<NodeId, std::list<std::shared_ptr<const CRecoveredSig>>> pendingRecoveredSigs GUARDED_BY(cs_pending);
    std::unordered_map<uint256, std::shared_ptr<const CRecoveredSig>, StaticSaltedHasher> pendingReconstructedRecoveredSigs GUARDED_BY(cs_pending);

    FastRandomContext rnd GUARDED_BY(cs_pending);

    int64_t lastCleanupTime{0};

    mutable Mutex cs_listeners;
    std::vector<CRecoveredSigsListener*> recoveredSigsListeners GUARDED_BY(cs_listeners);

public:
    SigningManagerImpl(CConnman& _connman, const CActiveMasternodeManager* const mn_activeman, const CChainState& chainstate,
                    const CQuorumManager& _qman, const std::unique_ptr<PeerManager>& peerman, bool fMemory, bool fWipe);

    bool AlreadyHave(const CInv& inv) const;
    bool GetRecoveredSigForGetData(const uint256& hash, CRecoveredSig& ret) const;

    PeerMsgRet ProcessMessage(const CNode& pnode, const std::string& msg_type, CDataStream& vRecv);

    // This is called when a recovered signature was was reconstructed from another P2P message and is known to be valid
    // This is the case for example when a signature appears as part of InstantSend or ChainLocks
    void PushReconstructedRecoveredSig(const std::shared_ptr<const CRecoveredSig>& recoveredSig);

    // This is called when a recovered signature can be safely removed from the DB. This is only safe when some other
    // mechanism prevents possible conflicts. As an example, ChainLocks prevent conflicts in confirmed TXs InstantSend votes
    // This won't completely remove all traces of the recovered sig but instead leave the hash entry in the DB. This
    // allows AlreadyHave to keep returning true. Cleanup will later remove the remains
    void TruncateRecoveredSig(Consensus::LLMQType llmqType, const uint256& id);

private:
    PeerMsgRet ProcessMessageRecoveredSig(const CNode& pfrom, const std::shared_ptr<const CRecoveredSig>& recoveredSig);

    void CollectPendingRecoveredSigsToVerify(size_t maxUniqueSessions,
            std::unordered_map<NodeId, std::list<std::shared_ptr<const CRecoveredSig>>>& retSigShares,
            std::unordered_map<std::pair<Consensus::LLMQType, uint256>, CQuorumCPtr, StaticSaltedHasher>& retQuorums);
    void ProcessPendingReconstructedRecoveredSigs();
    bool ProcessPendingRecoveredSigs(); // called from the worker thread of CSigSharesManager
public:
    // TODO - should not be public!
    void ProcessRecoveredSig(const std::shared_ptr<const CRecoveredSig>& recoveredSig);
private:
    void Cleanup(); // called from the worker thread of CSigSharesManager

public:
    // public interface
    void RegisterRecoveredSigsListener(CRecoveredSigsListener* l);
    void UnregisterRecoveredSigsListener(CRecoveredSigsListener* l);

    bool AsyncSignIfMember(Consensus::LLMQType llmqType, CSigSharesManager& shareman, const uint256& id,
                           const uint256& msgHash, const uint256& quorumHash = uint256(), bool allowReSign = false,
                           bool allowDiffMsgHashSigning = false);
    bool HasRecoveredSig(Consensus::LLMQType llmqType, const uint256& id, const uint256& msgHash) const;
    bool HasRecoveredSigForId(Consensus::LLMQType llmqType, const uint256& id) const;
    bool HasRecoveredSigForSession(const uint256& signHash) const;
    bool GetRecoveredSigForId(Consensus::LLMQType llmqType, const uint256& id, CRecoveredSig& retRecSig) const;
    bool IsConflicting(Consensus::LLMQType llmqType, const uint256& id, const uint256& msgHash) const;

    bool GetVoteForId(Consensus::LLMQType llmqType, const uint256& id, uint256& msgHashRet) const;

private:
    std::thread workThread;
    CThreadInterrupt workInterrupt;
    void WorkThreadMain();

public:
    void StartWorkerThread();
    void StopWorkerThread();
    void InterruptWorkerThread();
};

