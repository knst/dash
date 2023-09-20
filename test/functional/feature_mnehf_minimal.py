#!/usr/bin/env python3

# Copyright (c) 2023 The Dash Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import struct
from io import BytesIO
import time

from test_framework.authproxy import JSONRPCException
from test_framework.key import ECKey
from test_framework.messages import (
    CMnEhf,
    CTransaction,
    hash256,
    ser_string,
)

from test_framework.test_framework import DashTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    get_bip9_details,
)

# add fastDIP=3 activation - will it help ?
class MnehfTest(DashTestFramework):
    def set_test_params(self):
        self.set_dash_test_params(4, 3, fast_dip3_enforcement=True)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def restart_all_nodes(self):
        self.restart_node(0)
        for mn in self.mninfo:
            self.stop_node(mn.node.index)
            self.start_masternode(mn)
        for i in range(len(self.nodes)):
                self.log.info(f"connect {i} {0}...")
                self.connect_nodes(i, 0)


    def set_sporks(self):
        spork_enabled = 0
        spork_disabled = 4070908800

        self.nodes[0].sporkupdate("SPORK_17_QUORUM_DKG_ENABLED", spork_enabled)
        self.nodes[0].sporkupdate("SPORK_19_CHAINLOCKS_ENABLED", spork_disabled)
        self.nodes[0].sporkupdate("SPORK_3_INSTANTSEND_BLOCK_FILTERING", spork_disabled)
        self.nodes[0].sporkupdate("SPORK_2_INSTANTSEND_ENABLED", spork_disabled)
        self.wait_for_sporks_same()


    def run_test(self):
        node = self.nodes[0]

        self.set_sporks()
        self.activate_v19()
        self.log.info(f"After v19 activation should be plenty of blocks: {node.getblockcount()}")

        self.bump_mocktime(1)
        node.generate(1)
        self.sync_all()

        for node in self.nodes:
            self.log.info(f"test-before-first: {node.getpeerinfo()}")
        self.log.info("Mine a quorum...")
        self.mine_quorum()

        for node in self.nodes:
            self.log.info(f"test-after-first: {node.getpeerinfo()}")

        self.restart_all_nodes()

        for node in self.nodes:
            self.log.info(f"test-after-restart: {node.getpeerinfo()}")

        self.bump_mocktime(1)
        node.generate(1)
        self.sync_all()
        time.sleep(10)

        for node in self.nodes:
            self.log.info(f"test-after-restart+10: {node.getpeerinfo()}")

        self.log.info("Mine non-expected failing quorum...")
        self.mine_quorum()



if __name__ == '__main__':
    MnehfTest().main()
