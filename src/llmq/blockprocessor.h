// Copyright (c) 2018-2025 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_LLMQ_BLOCKPROCESSOR_H
#define BITCOIN_LLMQ_BLOCKPROCESSOR_H

#include <bls/bls.h>
#include <llmq/params.h>
#include <llmq/utils.h>
#include <msg_result.h>

#include <checkqueue.h>
#include <protocol.h>
#include <sync.h>

#include <gsl/pointers.h>

#include <optional>

class BlockValidationState;
class CBlock;
class CBlockIndex;
class CBLSSignature;
class CChain;
class Chainstate;
class ChainstateManager;
class CDataStream;
class CNode;
class EvoChainState;

extern RecursiveMutex cs_main; // NOLINT(readability-redundant-declaration)

namespace llmq
{
class CFinalCommitment;

class CQuorumBlockProcessor
{
private:
    ChainstateManager& m_chainman;

    CCheckQueue<utils::BlsCheck> m_bls_queue{4};

    mutable Mutex minableCommitmentsCs;
    std::map<std::pair<Consensus::LLMQType, uint256>, uint256> minableCommitmentsByQuorum GUARDED_BY(minableCommitmentsCs);
    std::map<uint256, CFinalCommitment> minableCommitments GUARDED_BY(minableCommitmentsCs);

public:
    CQuorumBlockProcessor() = delete;
    CQuorumBlockProcessor(const CQuorumBlockProcessor&) = delete;
    CQuorumBlockProcessor& operator=(const CQuorumBlockProcessor&) = delete;
    explicit CQuorumBlockProcessor(ChainstateManager& chainman, int8_t bls_threads);
    ~CQuorumBlockProcessor();

    [[nodiscard]] MessageProcessingResult ProcessMessage(const CNode& peer, std::string_view msg_type, CDataStream& vRecv)
        EXCLUSIVE_LOCKS_REQUIRED(!minableCommitmentsCs);

    bool ProcessBlock(Chainstate& chainstate, const CBlock& block, gsl::not_null<const CBlockIndex*> pindex, BlockValidationState& state,
                      bool fJustCheck, bool fBLSChecks) EXCLUSIVE_LOCKS_REQUIRED(::cs_main, !minableCommitmentsCs);
    bool UndoBlock(Chainstate& chainstate, const CBlock& block, gsl::not_null<const CBlockIndex*> pindex)
        EXCLUSIVE_LOCKS_REQUIRED(::cs_main, !minableCommitmentsCs);

    //! it returns hash of commitment if it should be relay, otherwise nullopt
    std::optional<CInv> AddMineableCommitment(const CFinalCommitment& fqc) EXCLUSIVE_LOCKS_REQUIRED(!minableCommitmentsCs);
    bool HasMineableCommitment(const uint256& hash) const EXCLUSIVE_LOCKS_REQUIRED(!minableCommitmentsCs);
    bool GetMineableCommitmentByHash(const uint256& commitmentHash, CFinalCommitment& ret) const
        EXCLUSIVE_LOCKS_REQUIRED(!minableCommitmentsCs);
    std::optional<std::vector<CFinalCommitment>> GetMineableCommitments(const Consensus::LLMQParams& llmqParams,
                                                                        int nHeight) const
        EXCLUSIVE_LOCKS_REQUIRED(::cs_main, !minableCommitmentsCs);
    bool GetMineableCommitmentsTx(const Consensus::LLMQParams& llmqParams, int nHeight, std::vector<CTransactionRef>& ret) const
        EXCLUSIVE_LOCKS_REQUIRED(::cs_main, !minableCommitmentsCs);
private:
    static bool GetCommitmentsFromBlock(const CBlock& block, gsl::not_null<const CBlockIndex*> pindex, std::multimap<Consensus::LLMQType, CFinalCommitment>& ret, BlockValidationState& state) EXCLUSIVE_LOCKS_REQUIRED(::cs_main);
    bool ProcessCommitment(Chainstate& chainstate, int nHeight, const uint256& blockHash, const CFinalCommitment& qc, BlockValidationState& state,
                           bool fJustCheck) EXCLUSIVE_LOCKS_REQUIRED(::cs_main, !minableCommitmentsCs);
public:
    // Public for multi-chainstate accounting tests and callers which validate
    // against a chainstate other than the active one.
    size_t GetNumCommitmentsRequired(const Consensus::LLMQParams& llmqParams, const CChain& chain, int nHeight) const
        EXCLUSIVE_LOCKS_REQUIRED(::cs_main);
private:
    static uint256 GetQuorumBlockHash(const Consensus::LLMQParams& llmqParams, const CChain& active_chain, int nHeight, int quorumIndex) EXCLUSIVE_LOCKS_REQUIRED(::cs_main);
    EvoChainState& ActiveEvo() const;
};
} // namespace llmq

#endif // BITCOIN_LLMQ_BLOCKPROCESSOR_H
