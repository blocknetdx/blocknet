#!/usr/bin/env python3
# Copyright (c) 2014 The Bitcoin Core developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#
# Exercise the wallet.  Ported from wallet.sh.
# Does the following:
#   a) creates 3 nodes, with an empty chain (no blocks).
#   b) node0 mines a block
#   c) node1 mines 32 blocks, so now node 0 has 60001phr, node 1 has 4250phr, node2 has none.
#   d) node0 sends 601 phr to node2, in two transactions (301 phr, then 300 phr).
#   e) node0 mines a block, collects the fee on the second transaction
#   f) node1 mines 16 blocks, to mature node0's just-mined block
#   g) check that node0 has 100-21, node2 has 21
#   h) node0 should now have 2 unspent outputs;  send these to node2 via raw tx broadcast by node1
#   i) have node1 mine a block
#   j) check balances - node0 should have 0, node2 should have 100
#

from test_framework.util import *
from test_case_base import TestCaseBase

class ZerocoinTest(TestCaseBase):
    def set_test_params(self):
        self.num_nodes = 3
        self.setup_clean_chain = True

    def test_wallet_tutorial(self):
        print("Mining blocks...")

        self.nodes[0].setgenerate(True, 33)
        self.sync_all()
        self.nodes[1].setgenerate(True, 33)
        self.sync_all()
        self.nodes[2].setgenerate(True, 34)
        self.sync_all()
        print("Node 0 minting 8 zerocoin")
        self.nodes[0].mintzerocoin(2)
        self.nodes[0].setgenerate(True, 15)
        self.nodes[0].mintzerocoin(2)
        self.nodes[0].setgenerate(True, 15)
        self.sync_all()
        print("Node 1 minting 8 zerocoin")
        self.nodes[1].mintzerocoin(4)
        self.nodes[1].setgenerate(True, 30)
        self.sync_all()
        print("Node 2 minting 8 zerocoin")
        self.nodes[2].mintzerocoin(4)
        self.nodes[2].setgenerate(True, 30)
        self.sync_all()
        print("Success!")
        print("Minting 280 blocks to get to 500")
        self.nodes[0].setgenerate(True, 350)
        self.sync_all()
        print("Attempting to spend after accumulator params change")
        self.nodes[1].spendzerocoin(1, False, False, 100)


if __name__ == '__main__':
    ZerocoinTest().main()
