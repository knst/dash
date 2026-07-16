#!/usr/bin/env python3
# Copyright (c) 2015-2025 The Dash Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

'''
Reconstruction-history divergence of operator-key uniqueness.

A masternode whose nVersion is bumped to BasicBLS (>= 2) by a post-v24 update while
its operator key stays legacy-encoded is indexed in the operator-key uniqueness map
under the LEGACY serialization on a node that applied the bump online (the map is not
re-keyed because the key value did not change), but under the BASIC serialization on a
node that reloads the list from a disk snapshot (which serializes the key to match
nVersion). A ProRegTx reusing that key basic-encoded is therefore accepted on the
online view and rejected as bad-protx-dup-key on the reloaded view: consensus
acceptance depends on reconstruction history, which is a chain-split risk.

This test uses a single node and restarts it, so "online" and "reloaded" are the same
node before and after the restart.
'''
from test_framework.test_framework import (
    DashTestFramework,
    MasternodeInfo,
)
from test_framework.util import (
    assert_equal,
    p2p_port,
    softfork_active,
)

# Full DMN list snapshots are written when height % DISK_SNAPSHOT_PERIOD == 0; a reload
# re-encodes the operator key only when it reconstructs from such a snapshot taken while
# the masternode is already at v2.
DISK_SNAPSHOT_PERIOD = 576


class ProTxDupKeyReloadDivergenceTest(DashTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser)

    def set_test_params(self):
        self.extra_args = [[
            '-deprecatedrpc=legacy_mn',
            '-testactivationheight=v19@200',
            f'-vbparams=v24:{self.mocktime}:999999999999:450:10:8:6:5:0',
        ]]
        self.set_dash_test_params(1, 0, extra_args=self.extra_args)

    def activate_v24(self):
        while not softfork_active(self.nodes[0], 'v24'):
            self.bump_mocktime(50)
            self.generate(self.nodes[0], 50, sync_fun=self.no_op)
        assert softfork_active(self.nodes[0], 'v24')

    def dup_verdict(self, node, pubkeyoperator, port):
        # Returns 'accepted' or 'rejected' for a ProRegTx reusing pubkeyoperator, without
        # changing chain state. register_fund(submit=False) still runs CheckSpecialTx, so the
        # bad-protx-dup-key verdict is reached; the built tx is never broadcast.
        mn = MasternodeInfo(evo=False, legacy=False)
        mn.generate_addresses(node)
        mn.pubKeyOperator = pubkeyoperator
        node.sendtoaddress(mn.fundsAddr, mn.get_collateral_value() + 1)
        self.bump_mocktime(10 * 60 + 1) # to make funds confirmable
        self.generate(node, 1)
        try:
            mn.register_fund(node, submit=False, addrs_core_p2p=[f'127.0.0.1:{port}'])
            return 'accepted'
        except Exception as e:
            assert 'bad-protx-dup-key' in str(e), str(e)
            return 'rejected'

    def run_test(self):
        node = self.nodes[0]
        base_port = p2p_port(len(self.nodes))

        self.log.info("Register a masternode before v19: operator key stored legacy-encoded")
        legacy_mn = MasternodeInfo(evo=False, legacy=True)
        legacy_mn.generate_addresses(node)
        node.sendtoaddress(legacy_mn.fundsAddr, legacy_mn.get_collateral_value() + 1)
        legacy_mn.set_params(proTxHash=legacy_mn.register_fund(
            node, submit=True, addrs_core_p2p=[f'127.0.0.1:{base_port}']))
        self.bump_mocktime(10 * 60 + 1) # to make tx safe to include in block
        self.generate(node, 1)
        assert_equal(node.protx('info', legacy_mn.proTxHash)['state']['version'], 1)

        self.activate_by_name('v19', expected_activation_height=200)
        self.activate_v24()

        self.log.info("update_service bumps the masternode to BasicBLS (v2), keeping the legacy key encoding")
        node.sendtoaddress(legacy_mn.fundsAddr, 1)
        legacy_mn.update_service(node, submit=True, addrs_core_p2p=[f'127.0.0.1:{base_port}'])
        self.bump_mocktime(10 * 60 + 1) # to make tx safe to include in block
        self.generate(node, 1)
        assert_equal(node.protx('info', legacy_mn.proTxHash)['state']['version'], 2)

        reused = node.bls('fromsecret', legacy_mn.keyOperator)['public']
        before = self.dup_verdict(node, reused, base_port + 1)

        self.log.info("Mine to a snapshot boundary so a full v2 snapshot is written, then restart")
        height = node.getblockcount()
        target = ((height // DISK_SNAPSHOT_PERIOD) + 1) * DISK_SNAPSHOT_PERIOD
        self.generate(node, target - height)
        self.restart_node(0, extra_args=self.extra_args[0])

        after = self.dup_verdict(node, reused, base_port + 2)

        self.log.info(f"Reused-key ProRegTx verdict: before restart={before}, after restart={after}")
        # Correct behavior: the verdict must not depend on reconstruction history.
        assert_equal(before, 'rejected')
        assert_equal(after, 'rejected')


if __name__ == '__main__':
    ProTxDupKeyReloadDivergenceTest().main()
