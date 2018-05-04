# The temp.py is used to only nail down the problems.
# After the problem is solved, the test code should be merge back
# to other formal test cases.

from test_framework.test_framework import BitcoinTestFramework
from test_framework.authproxy import AuthServiceProxy, JSONRPCException
from test_framework.util import *
from test_case_base import TestCaseBase


class TempTest(TestCaseBase) :
    def set_test_params(self):
        self.num_nodes = 2

    def initialize(self) :
        pass

    # Problem: node0 sends some coins to node1, after the transaction is
    # confirmed, node1 getbalance doesn't change.
    # The original test case is test_simple_send in segwit.py,
    # with this problem we can't know if sending to segwit address will
    # success or not.
    def test_simple_send(self):
        print("Block count:", self.nodes[0].getblockcount(), self.nodes[1].getblockcount())
        a = self.nodes[1].getbalance();
        print("A:", a)
        address = self.nodes[1].getnewaddress()
        self.nodes[0].sendtoaddress(address, 100)
        self.nodes[0].setgenerate(True, 101)
        sync_blocks(self.nodes)
        print("Block count:", self.nodes[0].getblockcount(), self.nodes[1].getblockcount())
        b = self.nodes[1].getbalance();
        print("B:", b)
        if b > a :
            print("Correct! B is larger than A")
        else :
            print("Wrong! B should be larger than A")

if __name__ == '__main__':
    TempTest().main()

