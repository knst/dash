#!/usr/bin/env python3

# Copyright (c) 2022 The Dash Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import copy
import struct
from decimal import Decimal
from io import BytesIO

from test_framework.blocktools import (
    create_block,
    create_coinbase,
)
from test_framework.authproxy import JSONRPCException
from test_framework.key import ECKey
from test_framework.messages import (
    COIN,
)
from test_framework.test_framework import DashTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_greater_than_or_equal,
    get_bip9_details,
)

llmq_type_test = 106 # LLMQType::LLMQ_TEST_PLATFORM

class TinyIS(DashTestFramework):
    def set_test_params(self):
        self.set_dash_test_params(4, 3)# , evo_count=3)
        self.extra_args = [["-llmqtestinstantsend=llmq_test", "-llmqtestinstantsenddip0024=llmq_test_instantsend"]] * 4

#llmqtestinstantsend=llmq_test
#llmqtestinstantsenddip0024=llmq_test_instantsend
    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def set_sporks(self):
        spork_enabled = 0
        spork_disabled = 4070908800

        self.nodes[0].sporkupdate("SPORK_21_QUORUM_ALL_CONNECTED", 0)
        self.nodes[0].sporkupdate("SPORK_17_QUORUM_DKG_ENABLED", spork_enabled)
        self.nodes[0].sporkupdate("SPORK_19_CHAINLOCKS_ENABLED", spork_disabled)
        self.nodes[0].sporkupdate("SPORK_3_INSTANTSEND_BLOCK_FILTERING", spork_disabled)
        self.nodes[0].sporkupdate("SPORK_2_INSTANTSEND_ENABLED", spork_disabled)
        self.wait_for_sporks_same()

    def slowly_generate_batch(self, amount):
        self.log.info(f"Slowly generate {amount} blocks")
        while amount > 0:
            self.log.info(f"Generating batch of blocks {amount} left")
            next = min(10, amount)
            amount -= next
            self.bump_mocktime(next)
            self.nodes[1].generate(next)
            self.sync_all()

    def run_test(self):
        node = self.nodes[1]

        self.set_sporks()

        self.activate_v19(expected_activation_height=900)
        self.log.info("Activated v19 at height:" + str(node.getblockcount()))


        for i in range(300):
            self.nodes[1].generate(1)
            self.sync_all()
            self.log.info(f"quorums: {node.quorum('list')}")
            self.log.info(f"v19: {node.getblockchaininfo()['softforks']['v19']}")
            self.log.info(f"v20: {node.getblockchaininfo()['softforks']['v20']}")

        self.log.info(f"quorums-a: {node.quorum('list')}")
#        self.mine_quorum()
        self.log.info(f"quorums-b: {node.quorum('list')}")
        self.mine_quorum(llmq_type_name='llmq_test_instantsend', llmq_type=104)
        self.log.info(f"quorums-c: {node.quorum('list')}")
        self.log.info("DONE")
        self.nodes[0].sporkupdate("SPORK_2_INSTANTSEND_ENABLED", 0)
        self.wait_for_sporks_same()
        for i in range(3):
            self.dynamically_add_masternode(evo=True)
            node.generate(8)
            self.sync_blocks()

        self.activate_v20()
        node.generate(1)
        self.sync_all()

        key = ECKey()
        key.generate()
        pubkey = key.get_pubkey().get_bytes()


        self.log.info("Mine a quorum...")
        self.mine_quorum(llmq_type_name='llmq_test_platform', llmq_type=106, expected_connections=2, expected_members=3, expected_contributions=3, expected_complaints=0, expected_justifications=0, expected_commitments=3 )
        node.quorum("list")



if __name__ == '__main__':
    TinyIS().main()
