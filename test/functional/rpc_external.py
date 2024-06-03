#!/usr/bin/env python3
# Copyright (c) 2024 The Dash Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import BitcoinTestFramework

class RPCExternalTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
#        self.extra_args = [["-rpcexternalthreads=1", "-rpcexternaluser=ext1", "-rpcuser=ext1", "-rpcpassword=123"], []]
        self.extra_args = [["-rpcexternalthreads=1", "-rpcexternaluser=ext1"], []]

    def add_options(self, parser):
        pass


    def run_test(self):
        self.log.info(f"node-0: {self.nodes[0].getblockchaininfo()}")
        self.log.info(f"node-1: {self.nodes[1].getblockchaininfo()}")
        rpc_ext = self.nodes[0].get_external_rpc()
        rpc_ext.rpcuser = "ext1"
        rpc_ext.rpcpassword = "123"
        self.log.info(f"ext: {rpc_ext}")
        self.log.info(f"node-0: {rpc_ext.getblockchaininfo()}")
        raise "error"

if __name__ == '__main__':
    RPCExternalTest().main()
