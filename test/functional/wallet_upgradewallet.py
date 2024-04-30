#!/usr/bin/env python3
# Copyright (c) 2018-2020 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""upgradewallet RPC functional test

Requires previous releases binaries, see test/README.md.
Only v0.15.2 and v0.16.3 are required by this test. The others are used in feature_backwards_compatibility.py
"""

import os
import shutil
import struct

from io import BytesIO

from test_framework.bdb import dump_bdb_kv
from test_framework.blocktools import COINBASE_MATURITY
from test_framework.messages import deser_compact_size, deser_string
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_greater_than_or_equal,
    assert_is_hex_string,
    assert_raises_rpc_error,
    sha256sum_file,
)


UPGRADED_KEYMETA_VERSION = 12

class UpgradeWalletTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 3
        self.extra_args = [
            ["-keypool=2"],                        # current wallet version
            ["-usehd=1", "-keypool=2"],            # v18.2.2 wallet
            ["-usehd=0", "-keypool=2"],             # v0.16.1.1 wallet
        ]
        self.wallet_names = [self.default_wallet_name, None, None]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()
        self.skip_if_no_bdb()
        self.skip_if_no_previous_releases()

    def setup_nodes(self):
        self.add_nodes(self.num_nodes, extra_args=self.extra_args, versions=[
            None,
            18020200, # that's last version before `default wallets` are created
            160101,   # that's oldest version that support `import_deterministic_coinbase_privkeys`
        ])
        self.start_nodes()
        self.import_deterministic_coinbase_privkeys()

    def dumb_sync_blocks(self):
        """
        Little helper to sync older wallets.
        Notice that v0.15.2's regtest is hardforked, so there is
        no sync for it.
        v0.15.2 is only being used to test for version upgrade
        and master hash key presence.
        v0.16.3 is being used to test for version upgrade and balances.
        Further info: https://github.com/bitcoin/bitcoin/pull/18774#discussion_r416967844
        """
        node_from = self.nodes[0]
        v18_2_node = self.nodes[1]
        to_height = node_from.getblockcount()
        height = self.nodes[1].getblockcount()
        for i in range(height, to_height+1):
            b = node_from.getblock(blockhash=node_from.getblockhash(i), verbose=0)
            v18_2_node.submitblock(b)
        assert_equal(v18_2_node.getblockcount(), to_height)

    def run_test(self):
        self.nodes[0].generatetoaddress(COINBASE_MATURITY + 1, self.nodes[0].getnewaddress())
        self.dumb_sync_blocks()
        # # Sanity check the test framework:
        res = self.nodes[0].getblockchaininfo()
        assert_equal(res['blocks'], COINBASE_MATURITY + 1)
        node_master = self.nodes[0]
        v18_2_node  = self.nodes[1]
        v16_1_node  = self.nodes[2]

        # Send coins to old wallets for later conversion checks.
        v18_2_wallet  = v18_2_node.get_wallet_rpc(self.default_wallet_name)
        v18_2_address = v18_2_wallet.getnewaddress()
        node_master.generatetoaddress(COINBASE_MATURITY + 1, v18_2_address)
        self.dumb_sync_blocks()
        v18_2_balance = v18_2_wallet.getbalance()

        self.log.info("Test upgradewallet RPC...")
        # Prepare for copying of the older wallet
        node_master_wallet_dir = os.path.join(node_master.datadir, "regtest/wallets", self.default_wallet_name)
        node_master_wallet = os.path.join(node_master_wallet_dir, self.default_wallet_name, self.wallet_data_filename)
        v18_2_wallet       = os.path.join(v18_2_node.datadir, "regtest/wallets/wallet.dat")
        v16_1_wallet       = os.path.join(v16_1_node.datadir, "regtest/wallets/wallet.dat")
        self.stop_nodes()

        shutil.rmtree(node_master_wallet_dir)
        os.mkdir(node_master_wallet_dir)
        shutil.copy(
            v18_2_wallet,
            node_master_wallet_dir
        )
        self.restart_node(0, ['-nowallet'])
        node_master.loadwallet('')

        def copy_v16():
            node_master.get_wallet_rpc(self.default_wallet_name).unloadwallet()
            # Copy the 0.16.3 wallet to the last Dash Core version and open it:
            shutil.rmtree(node_master_wallet_dir)
            os.mkdir(node_master_wallet_dir)
            shutil.copy(
                    v18_2_wallet,
                node_master_wallet_dir
            )
            node_master.loadwallet(self.default_wallet_name)

        def copy_non_hd():
            node_master.get_wallet_rpc(self.default_wallet_name).unloadwallet()
            # Copy the 19.3.0 wallet to the last Dash Core version and open it:
            shutil.rmtree(node_master_wallet_dir)
            os.mkdir(node_master_wallet_dir)
            shutil.copy(
                v16_1_wallet,
                node_master_wallet_dir
            )
            node_master.loadwallet(self.default_wallet_name)

        def copy_split_hd():
            node_master.get_wallet_rpc(self.default_wallet_name).unloadwallet()
            # Copy the 0.15.2 split hd wallet to the last Bitcoin Core version and open it:
            shutil.rmtree(node_master_wallet_dir)
            os.mkdir(node_master_wallet_dir)
            shutil.copy(
                split_hd_wallet,
                os.path.join(node_master_wallet_dir, 'wallet.dat')
            )
            node_master.loadwallet(self.default_wallet_name)

        self.restart_node(0)
        copy_v16()
        wallet = node_master.get_wallet_rpc(self.default_wallet_name)
        old_version = wallet.getwalletinfo()["walletversion"]

        # calling upgradewallet without version arguments
        # should return nothing if successful
        assert_equal(wallet.upgradewallet(), {})
        new_version = wallet.getwalletinfo()["walletversion"]
        # upgraded wallet version should be greater than older one
        assert_greater_than_or_equal(new_version, old_version)
        # wallet should still contain the same balance
        assert_equal(wallet.getbalance(), v18_2_balance)

        copy_non_hd()
        wallet = node_master.get_wallet_rpc(self.default_wallet_name)
        # should have no master key hash before conversion
        assert_equal('hdchainid' in wallet.getwalletinfo(), False)
        # calling upgradewallet with explicit version number
        # should return nothing if successful

        assert_equal(wallet.upgradewallet(169900), {})
        new_version = wallet.getwalletinfo()["walletversion"]
        # upgraded wallet would have 120200 but no HD seed actually appeared
        assert_equal(120200, new_version)
        # after conversion master key hash should not be present yet
        assert 'hdchainid' not in wallet.getwalletinfo()
        assert_equal(wallet.upgradetohd(), True)
        new_version = wallet.getwalletinfo()["walletversion"]
        assert_equal(new_version, 120200)
        assert_is_hex_string(wallet.getwalletinfo()['hdchainid'])

        self.log.info('Intermediary versions don\'t effect anything')
        copy_non_hd()
        # Wallet starts with 61000 (legacy "latest")
        assert_equal(61000, wallet.getwalletinfo()['walletversion'])
        wallet.unloadwallet()
        before_checksum = sha256sum_file(node_master_wallet)
        node_master.loadwallet('')
        # Can "upgrade" to 120199 which should have no effect on the wallet
        wallet.upgradewallet(120199)
        assert_equal(61000, wallet.getwalletinfo()['walletversion'])
        wallet.unloadwallet()
        assert_equal(before_checksum, sha256sum_file(node_master_wallet))
        node_master.loadwallet('')

        self.log.info('Wallets cannot be downgraded')
        copy_non_hd()
        assert_raises_rpc_error(-4, 'Cannot downgrade wallet', wallet.upgradewallet, 40000)
        wallet.unloadwallet()
        assert_equal(before_checksum, sha256sum_file(node_master_wallet))
        node_master.loadwallet('')

        self.log.info('Can upgrade to HD')
        # Inspect the old wallet and make sure there is no hdchain
        orig_kvs = dump_bdb_kv(node_master_wallet)
        assert b'\x07hdchain' not in orig_kvs
        # Upgrade to HD
        wallet.upgradewallet(120200)
        assert_equal(120200, wallet.getwalletinfo()['walletversion'])
        # Check that there is now a hd chain and it is version 1, no internal chain counter
        new_kvs = dump_bdb_kv(node_master_wallet)
        wallet.upgradetohd()
        new_kvs = dump_bdb_kv(node_master_wallet)
        assert b'\x07hdchain' in new_kvs
        hd_chain = new_kvs[b'\x07hdchain']
        assert_greater_than(220, len(hd_chain))
        assert_greater_than(len(hd_chain), 180)
#                obj.nVersion   int
#                obj.id         uint256
#                obj.fCrypted   bool
#                obj.vchSeed    SecureVector
#                obj.vchMnemonic SecureVector
#                obj.vchMnemonicPassphrase SecureVector
#                obj.mapAccounts #map accounts
# FIX FIX TODO
        print(f"chain: {hd_chain}")
        hd_chain_version, seed_id, is_crypted = struct.unpack('<i32s?', hd_chain[:37])
        print(f"everything: {hd_chain_version}, {seed_id} {is_crypted}")
        assert_equal(1, hd_chain_version)
        seed_id = bytearray(seed_id)
        seed_id.reverse()
        old_kvs = new_kvs
        # First 2 keys should still be non-HD
#        for i in range(0, 2):
        for i in range(0, 0):
            info = wallet.getaddressinfo(wallet.getnewaddress())
            self.log.info(f"keypath: {info}")
            assert 'hdkeypath' not in info
            assert 'hdchainid' not in info
        # Next key should be HD
        info = wallet.getaddressinfo(wallet.getnewaddress())
        assert_equal(seed_id.hex(), info['hdchainid'])
        assert_equal("m/44'/1'/0'/0/0", info['hdkeypath'])
        prev_seed_id = info['hdchainid']
        # Change key should be the same keypool
        info = wallet.getaddressinfo(wallet.getrawchangeaddress())
        assert_equal(prev_seed_id, info['hdchainid'])
        assert_equal("m/44'/1'/0'/1/0", info['hdkeypath'])

        assert_equal(120200, wallet.getwalletinfo()['walletversion'])


if __name__ == '__main__':
    UpgradeWalletTest().main()
