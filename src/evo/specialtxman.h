// Copyright (c) 2018-2023 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_EVO_SPECIALTXMAN_H
#define BITCOIN_EVO_SPECIALTXMAN_H

#include <primitives/transaction.h>
#include <sync.h>
#include <threadsafety.h>

#include <optional>

class BlockValidationState;
class CBlock;
class CBlockIndex;
class CCoinsViewCache;
class CMNHFManager;
class TxValidationState;
namespace llmq {
class CQuorumBlockProcessor;
class CChainLocksHandler;
} // namespace llmq
namespace Consensus {
struct Params;
} // namespace Consensus

extern RecursiveMutex cs_main;


#endif // BITCOIN_EVO_SPECIALTXMAN_H
