#!/usr/bin/env python3
# Copyright (c) 2015-2024 The Dash Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

'''
feature_llmq_signing.py

Checks LLMQs signing sessions

'''

from test_framework.messages import CSigShare, msg_qsigshare, uint256_to_string
from test_framework.p2p import P2PInterface
from test_framework.test_framework import DashTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error, force_finish_mnsync, wait_until_helper


class LLMQSigningTest(DashTestFramework):
    def set_test_params(self):
        self.set_dash_test_params(5, 4)

    def add_options(self, parser):
        parser.add_argument("--spork21", dest="spork21", default=False, action="store_true",
                            help="Test with spork21 enabled")

    def run_test(self):

        self.nodes[0].sporkupdate("SPORK_17_QUORUM_DKG_ENABLED", 0)
        if self.options.spork21:
            self.nodes[0].sporkupdate("SPORK_21_QUORUM_ALL_CONNECTED", 0)
        self.wait_for_sporks_same()

        self.mine_quorum()

        if self.options.spork21:
            assert self.mninfo[0].node.getconnectioncount() == self.llmq_size

        id = "0000000000000000000000000000000000000000000000000000000000000001"
        msgHash = "0000000000000000000000000000000000000000000000000000000000000002"
        msgHashConflict = "0000000000000000000000000000000000000000000000000000000000000003"

        def check_sigs(hasrecsigs, isconflicting1, isconflicting2):
            for mn in self.mninfo:
                if mn.node.quorum("hasrecsig", 111, id, msgHash) != hasrecsigs:
                    self.log.info(f'hasrecsig {mn.node.quorum("hasrecsig", 111, id, msgHash)} {hasrecsigs}')
                    return False
                if mn.node.quorum("isconflicting", 111, id, msgHash) != isconflicting1:
                    self.log.info(f'is conflicting-1 {mn.node.quorum("isconflicting", 111, id, msgHash)} {isconflicting1}')
                    return False
                if mn.node.quorum("isconflicting", 111, id, msgHashConflict) != isconflicting2:
                    self.log.info(f'is conflicting-2 {mn.node.quorum("isconflicting", 111, id, msgHashConflict)} {isconflicting2}')
                    return False
            return True

        def wait_for_sigs(hasrecsigs, isconflicting1, isconflicting2, timeout):
            self.wait_until(lambda: check_sigs(hasrecsigs, isconflicting1, isconflicting2), timeout = timeout)

        def assert_sigs_nochange(hasrecsigs, isconflicting1, isconflicting2, timeout):
            assert not wait_until_helper(lambda: not check_sigs(hasrecsigs, isconflicting1, isconflicting2), timeout = timeout, do_assert = False)

        # Initial state
        wait_for_sigs(False, False, False, 1)

        # Sign first share without any optional parameter, should not result in recovered sig
        # Sign second share and test optional quorumHash parameter, should not result in recovered sig
        # 1. Providing an invalid quorum hash should fail and cause no changes for sigs
        assert not self.mninfo[1].node.quorum("sign", 111, id, msgHash, msgHash)
        assert_sigs_nochange(False, False, False, 3)
        # 2. Providing a valid quorum hash should succeed and cause no changes for sigss
        quorumHash = self.mninfo[1].node.quorum("selectquorum", 111, id)["quorumHash"]
        self.mninfo[0].node.quorum("sign", 111, id, msgHash)
        sign1 = self.mninfo[0].node.quorum("sign", 111, id, msgHash, quorumHash) 
        sign2 = self.mninfo[1].node.quorum("sign", 111, id, msgHash, quorumHash) 
        sign3 = self.mninfo[2].node.quorum("sign", 111, id, msgHash, quorumHash) 
        sign4 = self.mninfo[3].node.quorum("sign", 111, id, msgHash, quorumHash) 
        self.log.info(f"signs: {sign1} {sign2} {sign3} {sign4}")

        wait_for_sigs(True, False, True, 15)

        if self.options.spork21:
            mn.node.disconnect_p2ps()

        # Test `quorum verify` rpc
        node = self.mninfo[0].node
        recsig = node.quorum("getrecsig", 111, id, msgHash)
        # Find quorum automatically
        height = node.getblockcount()
        height_bad = node.getblockheader(recsig["quorumHash"])["height"]
        hash_bad = node.getblockhash(0)
        assert node.quorum("verify", 111, id, msgHash, recsig["sig"])
        assert node.quorum("verify", 111, id, msgHash, recsig["sig"], "", height)
        assert not node.quorum("verify", 111, id, msgHashConflict, recsig["sig"])
        assert not node.quorum("verify", 111, id, msgHash, recsig["sig"], "", height_bad)
        # Use specific quorum
        assert node.quorum("verify", 111, id, msgHash, recsig["sig"], recsig["quorumHash"])
        assert not node.quorum("verify", 111, id, msgHashConflict, recsig["sig"], recsig["quorumHash"])
        assert_raises_rpc_error(-8, "quorum not found", node.quorum, "verify", 111, id, msgHash, recsig["sig"], hash_bad)

        # Mine one more quorum, so that we have 2 active ones, nothing should change
        self.mine_quorum()
        assert_sigs_nochange(True, False, True, 3)

        # Create a recovered sig for the oldest quorum i.e. the active quorum which will be moved
        # out of the active set when a new quorum appears
        request_id = 2
        oldest_quorum_hash = node.quorum("list")["llmq_test_instantsend"][-1]
        # Search for a request id which selects the last active quorum
        while True:
            selected_hash = node.quorum('selectquorum', 111, uint256_to_string(request_id))["quorumHash"]
            if selected_hash == oldest_quorum_hash:
                break
            else:
                request_id += 1
        # Produce the recovered signature
        id = uint256_to_string(request_id)
        for mn in self.mninfo:
            mn.node.quorum("sign", 111, id, msgHash)
        # And mine a quorum to move the quorum which signed out of the active set
        self.mine_quorum()
        # Verify the recovered sig. This triggers the "signHeight + dkgInterval" verification
        recsig = node.quorum("getrecsig", 111, id, msgHash)
        assert node.quorum("verify", 111, id, msgHash, recsig["sig"], "", node.getblockcount())

        recsig_time = self.mocktime

        # Mine 2 more quorums, so that the one used for the the recovered sig should become inactive, nothing should change
        self.mine_quorum()
        self.mine_quorum()
        assert_sigs_nochange(True, False, True, 3)

        # fast forward until 0.5 days before cleanup is expected, recovered sig should still be valid
        self.bump_mocktime(recsig_time + int(60 * 60 * 24 * 6.5) - self.mocktime, update_schedulers=False)
        # Cleanup starts every 5 seconds
        wait_for_sigs(True, False, True, 15)
        # fast forward 1 day, recovered sig should not be valid anymore
        self.bump_mocktime(int(60 * 60 * 24 * 1), update_schedulers=False)
        # Cleanup starts every 5 seconds
        wait_for_sigs(False, False, False, 15)

        for i in range(2):
            self.mninfo[i].node.quorum("sign", 111, id, msgHashConflict)
        for i in range(2, 4):
            self.mninfo[i].node.quorum("sign", 111, id, msgHash)
        wait_for_sigs(True, False, True, 15)

        if self.options.spork21:
            id = uint256_to_string(request_id + 1)

            # Isolate the node that is responsible for the recovery of a signature and assert that recovery fails
            q = self.nodes[0].quorum('selectquorum', 111, id)
            mn = self.get_mninfo(q['recoveryMembers'][0])
            mn.node.setnetworkactive(False)
            self.wait_until(lambda: mn.node.getconnectioncount() == 0)
            for i in range(4):
                self.mninfo[i].node.quorum("sign", 111, id, msgHash)
            assert_sigs_nochange(False, False, False, 3)
            # Need to re-connect so that it later gets the recovered sig
            mn.node.setnetworkactive(True)
            self.connect_nodes(mn.node.index, 0)
            force_finish_mnsync(mn.node)
            # Make sure intra-quorum connections were also restored
            self.bump_mocktime(1)  # need this to bypass quorum connection retry timeout
            self.wait_until(lambda: mn.node.getconnectioncount() == self.llmq_size, timeout=10)
            mn.node.ping()
            self.wait_until(lambda: all('pingwait' not in peer for peer in mn.node.getpeerinfo()))
            # Let 2 seconds pass so that the next node is used for recovery, which should succeed
            self.bump_mocktime(2)
            wait_for_sigs(True, False, True, 2)

if __name__ == '__main__':
    LLMQSigningTest().main()
