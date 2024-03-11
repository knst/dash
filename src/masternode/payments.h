// Copyright (c) 2014-2023 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_MASTERNODE_PAYMENTS_H
#define BITCOIN_MASTERNODE_PAYMENTS_H

#include <amount.h>


/**
 * Masternode Payments Namespace
 * Helpers to kees track of who should get paid for which blocks
 */

namespace MasternodePayments
{
/**
 * this helper returns amount that should be reallocated to platform
 * it is calculated based on total amount of Masternode rewards (not block reward)
 */
CAmount PlatformShare(const CAmount masternodeReward);

} // namespace MasternodePayments

#endif // BITCOIN_MASTERNODE_PAYMENTS_H
