#!/usr/bin/env python3
# Copyright (c) 2022-2025 The Dash Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
from test_framework.test_framework import (
    DashTestFramework,
    MasternodeInfo,
)
from test_framework.util import assert_equal

'''
rpc_quorum.py

Test "quorum" rpc subcommands
'''

class RPCMasternodeTest(DashTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser)

    def set_test_params(self):
        # A single-member quorum exercises the same per-member `quorum info`
        # output the test asserts, so one masternode is sufficient.
        self.set_dash_test_params(2, 1)
        self.set_dash_llmq_test_params(1, 1)

    def run_test(self):
        self.test_active_dkg_membership()

        self.nodes[0].sporkupdate("SPORK_17_QUORUM_DKG_ENABLED", 0)
        self.wait_for_sporks_same()
        self.mine_until_mns_confirmed_for_next_dkg()
        quorum_hash = self.mine_quorum_single_member()

        quorum_info = self.nodes[0].quorum("info", 100, quorum_hash)
        for idx in range(0, self.mn_count):
            mn: MasternodeInfo = self.mninfo[idx]
            for member in quorum_info["members"]:
                if member["proTxHash"] == mn.proTxHash:
                    assert_equal(member['addresses']['core_p2p'][0], f'127.0.0.1:{mn.nodePort}')

    def test_active_dkg_membership(self):
        # Keep DKG disabled so no local session record can hide a reporting gap.
        # Membership must remain counted for the whole current window anyway.
        self.nodes[0].sporkupdate("SPORK_17_QUORUM_DKG_ENABLED", 4070908800)
        self.wait_for_sporks_same()
        self.mine_until_mns_confirmed_for_next_dkg()
        mn = self.mninfo[0]
        node = mn.get_node(self)
        other = "01" * 32
        tip = self.nodes[0].getblockcount()
        start_height = tip + 24 - tip % 24
        self.generate(self.nodes[0], start_height - tip - 1)
        info = node.quorum("dkginfo")
        assert_equal(info["active_dkgs"], 0)
        upcoming = info["upcoming_dkgs"]
        other_upcoming = node.quorum("dkginfo", other)["upcoming_dkgs"]
        [llmq_test] = [d for d in upcoming if d["llmqType"] == 100]
        assert_equal(llmq_test["blocksUntilStart"], 1)
        assert_equal(llmq_test["known"], True)
        assert_equal(llmq_test["isMember"], True)
        [other_llmq_test] = [d for d in other_upcoming if d["llmqType"] == 100]
        assert_equal(other_llmq_test["known"], True)
        assert_equal(other_llmq_test["isMember"], False)

        for offset in (0, 1, 9, 10):
            height = start_height + offset
            self.generate(self.nodes[0], height - self.nodes[0].getblockcount())
            info = node.quorum("dkginfo")
            if offset < 10:
                assert info["active_dkgs"] > 0
            assert_equal(info["active_dkgs"], self.expected_active_dkgs(upcoming, height))
            assert_equal(node.quorum("dkginfo", other)["active_dkgs"], self.expected_active_dkgs(other_upcoming, height))
            assert_equal(node.quorum("dkgstatus")["session"], [])
            assert all(d["blocksUntilStart"] > 0 for d in info["upcoming_dkgs"])


if __name__ == '__main__':
    RPCMasternodeTest().main()
