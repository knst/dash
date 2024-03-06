// Copyright (c) 2014-2024 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_EVO_CHAINHELPER_H
#define BITCOIN_EVO_CHAINHELPER_H

#include <masternode/payments.h>

class CChainstateHelper : public CMNPaymentsProcessor
{
public:
    explicit CChainstateHelper(CGovernanceManager& govman, const Consensus::Params& consensus_params, const CMasternodeSync& mn_sync,
                               const CSporkManager& sporkman) :
        CMNPaymentsProcessor(govman, consensus_params, mn_sync, sporkman) {}
};

#endif // BITCOIN_EVO_CHAINHELPER_H
