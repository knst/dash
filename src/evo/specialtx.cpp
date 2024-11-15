// Copyright (c) 2018-2023 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <evo/specialtx.h>

#include <clientversion.h>
#include <hash.h>

#include <logging.h>
uint256 CalcTxInputsHash(const CTransaction& tx)
{
    /*
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    for (const auto& in : tx.vin) {
        ::Serialize(oss, in.prevout);
    }
    LogPrintf("data: %s\n", oss.str());
*/
    LogPrintf("data starts:\n");
    CHashWriter hw(SER_GETHASH, CLIENT_VERSION);
    for (const auto& in : tx.vin) {
        LogPrintf("next-input:\n");
        hw << in.prevout;
    }
//    LogPrintf("hw string: %s\n", hw.ToString());
    return hw.GetHash();
}
