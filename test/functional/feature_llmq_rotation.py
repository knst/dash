#!/usr/bin/env python3
# Copyright (c) 2015-2024 The Dash Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

'''
feature_llmq_rotation.py

Checks LLMQs Quorum Rotation

'''
import struct
from io import BytesIO

from test_framework.test_framework import DashTestFramework
from test_framework.messages import CBlock, CBlockHeader, CCbTx, CMerkleBlock, from_hex, hash256, msg_getmnlistd, QuorumId, ser_uint256, sha256
from test_framework.p2p import P2PInterface
from test_framework.util import (
    assert_equal,
    assert_greater_than_or_equal,
)


class LLMQQuorumRotationTest(DashTestFramework):
    def set_test_params(self):
        self.set_dash_test_params(9, 8)
        self.set_dash_llmq_test_params(4, 4)

    def run_test(self):
        self.nodes[0].sporkupdate("SPORK_17_QUORUM_DKG_ENABLED", 0)
        self.wait_for_sporks_same()

        #Mine 2 quorums so that Chainlocks can be available: Need them to include CL in CbTx as soon as v20 activates
        self.log.info("Mining 2 quorums")
        h_0 = self.mine_quorum()
        h_1 = self.mine_quorum()


if __name__ == '__main__':
    LLMQQuorumRotationTest().main()
