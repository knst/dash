#!/usr/bin/env python3
# Copyright (c) 2024 The Dash Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import str_to_b64str, assert_equal

import http.client
import json
import os
import urllib.parse

class RPCExternalTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
#        self.extra_args = [["-rpcexternalthreads=1", "-rpcexternaluser=ext1", "-rpcuser=ext1", "-rpcpassword=123"], []]
        self.extra_args = [["-rpcexternalthreads=1", "-rpcexternaluser=platform-user"], []]


    def setup_chain(self):
        super().setup_chain()
        # Append rpcauth to dash.conf before initialization
        rpcauthplatform = "rpcauth=platform-user:dd88fd676186f48553775d6fb5a2d344$bc1f7898698ead19c6ec7ff47055622dd7101478f1ff6444103d3dc03cd77c13"
        # rpcuser : platform-user
        # rpcpassword : password123
        rpcauthoperator = "rpcauth=operator:e9b45dd0b61a7be72155535435365a3a$8fb7470bc6f74d8ceaf9a23f49b06127723bd563b3ed5d9cea776ef01803d191"
        # rpcuser : operator
        # rpcpassword : otherpassword

#    masternodeblskey="masternodeblsprivkey=58af6e39bb4d86b22bda1a02b134c2f5b71caffa1377540b02f7f1ad122f59e0"

        with open(os.path.join(self.options.tmpdir+"/node0", "dash.conf"), 'a', encoding='utf8') as f:
#            f.write(masternodeblskey+"\n")
            f.write(rpcauthplatform+"\n")
            f.write(rpcauthoperator+"\n")

    def test_command(self, method, params, external, auth, expexted_status, should_not_match=False):
        url = urllib.parse.urlparse(self.nodes[0].url)
        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()
        body = {"method": method}
        if len(params):
            body["params"] = params
        path = '/' if not external else '/external'
        conn.request('POST', path, json.dumps(body), {"Authorization": "Basic " + str_to_b64str(auth)})
        resp = conn.getresponse()
        if should_not_match:
            assert resp.status != expexted_status
        else:
            assert_equal(resp.status, expexted_status)
        self.log.info(f"response: {resp.read().decode()}")
        conn.close()

    def run_test(self):
        self.log.info(f"node-0: {self.nodes[0].getblockchaininfo()}")
        self.log.info(f"node-1: {self.nodes[1].getblockchaininfo()}")


        rpcuser_authpair_platform = "platform-user:password123"
        rpcuser_authpair_operator = "operator:otherpassword"

        self.test_command("getblockchaininfo", [], True, rpcuser_authpair_platform, 200)
        self.test_command("getblockchaininfo", [], False, rpcuser_authpair_operator, 200)
        self.test_command("getblockchaininfo", [], False, rpcuser_authpair_platform, 403)

if __name__ == '__main__':
    RPCExternalTest().main()
