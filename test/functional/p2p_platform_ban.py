#!/usr/bin/env python3
# Copyright (c) 2025 The Dash Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.key import ECKey
from test_framework.messages import msg_platformban, hash256, ser_string, ser_uint256
from test_framework.p2p import (
    p2p_lock,
    P2PInterface,
)
from test_framework.test_framework import DashTestFramework
from test_framework.util import wait_until_helper
from test_framework.wallet_util import bytes_to_wif

import struct

class PlatformBanInterface(P2PInterface):
    def __init__(self):
        super().__init__()


class PlatformBanMessagesTest(DashTestFramework):
    def set_test_params(self):
#self.set_dash_test_params(1, 0, [[]], evo_count=2)
        self.set_dash_test_params(1, 0, [[]], evo_count=4)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def check_banned(self, mn):
        info = self.nodes[0].protx('info', mn.proTxHash)
        if info['state']['PoSeBanHeight'] != -1:
            return True
        return False

    def run_test(self):
        node = self.nodes[0]

        node.sporkupdate("SPORK_17_QUORUM_DKG_ENABLED", 0)
        node.sporkupdate("SPORK_2_INSTANTSEND_ENABLED", 1)
        self.wait_for_sporks_same()

        for _ in range(4):
            self.dynamically_add_masternode(evo=True)

#        self.activate_v20(expected_activation_height=900)
#        self.log.info("Activated v20 at height:" + str(node.getblockcount()))

        self.mempool_size = 0

        key = ECKey()
        key.generate()
        privkey = bytes_to_wif(key.get_bytes())
        node.importprivkey(privkey)
        pubkey = key.get_pubkey().get_bytes()

        self.mine_quorum(llmq_type_name='llmq_test_platform', llmq_type=106)


        self.log.info("Create and sign platform-ban message for mn-0")
        msg = msg_platformban()
        msg.protx_hash = int(self.mninfo[0].proTxHash, 16)
        msg.signed_height = node.getblockcount()
        #msg.signed_height = node.getblockcount() --- maybe shold be height of quorum

        request_id_buf = ser_string(b"PlatformPoSeBan") + ser_uint256(msg.protx_hash) + struct.pack("<I", msg.signed_height)
        request_id = hash256(request_id_buf)[::-1].hex()

        quorum_hash = self.mninfo[1].node.quorum("selectquorum", 106, request_id)["quorumHash"]
        msg.quorum_hash = int(quorum_hash, 16)

        msg_hash = format(msg.calc_sha256(), '064x')

        recsig = self.get_recovered_sig(request_id, msg_hash, llmq_type=106, use_platformsign=True)
#self.log.info(f"sig: {recsig}")
        msg.sig = bytearray.fromhex(recsig["sig"])

        self.log.info(f"Platform ban message is created: {msg}")

        p2p_node2 = self.mninfo[1].node.add_p2p_connection(PlatformBanInterface())
        p2p_node2.send_message(msg)
        wait_until_helper(lambda: p2p_node2.message_count["platformban"] > 0, timeout=10, lock=p2p_lock)
        p2p_node2.message_count[message] = 0

if __name__ == '__main__':
    PlatformBanMessagesTest().main()
