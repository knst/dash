// Copyright (c) 2024 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_EVO_CHAINHELPER_H
#define BITCOIN_EVO_CHAINHELPER_H

#include <memory>

class CMNPaymentsProcessor;
class CMasternodeSync;
class CGovernanceManager;
class CSporkManager;
namespace Consensus
{
    class Params;
}

class CChainstateHelper
{
public:
    explicit CChainstateHelper(CGovernanceManager& govman, const Consensus::Params& consensus_params, const CMasternodeSync& mn_sync,
                               const CSporkManager& sporkman);
    ~CChainstateHelper();
public:
    const std::unique_ptr<CMNPaymentsProcessor> mnPayments;
};

#endif // BITCOIN_EVO_CHAINHELPER_H
