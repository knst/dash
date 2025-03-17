#!/usr/bin/env python3
# Copyright (c) 2025 The Dash Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#class PlatformBanInterface(P2PInterface):


from test_framework.test_framework import DashTestFramework
from test_framework.key import ECKey
from test_framework.wallet_util import bytes_to_wif

class PlatformBanMessagesTest(DashTestFramework):
    def set_test_params(self):
#self.set_dash_test_params(1, 0, [[]], evo_count=2)
        self.set_dash_test_params(1, 0, evo_count=3)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        #self.activate_v20(expected_activation_height=900)
        #self.log.info("Activated v20 at height:" + str(node.getblockcount()))
        node = self.nodes[0]
        for _ in range(3):
            self.dynamically_add_masternode(evo=True)

        self.mempool_size = 0

        key = ECKey()
        key.generate()
        privkey = bytes_to_wif(key.get_bytes())
        node.importprivkey(privkey)
        pubkey = key.get_pubkey().get_bytes()


if __name__ == '__main__':
    PlatformBanMessagesTest().main()
