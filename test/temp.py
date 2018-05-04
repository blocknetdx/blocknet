# The temp.py is used to only nail down the problems.
# After the problem is solved, the test code should be merge back
# to other formal test cases.

from test_framework.test_framework import BitcoinTestFramework
from test_framework.authproxy import AuthServiceProxy, JSONRPCException
from test_framework.util import *
from test_case_base import TestCaseBase


def send_raw_transaction(node, utxo, to_address, amount) :
    inputs = []
    outputs = {}
    inputs.append({ "txid" : utxo["txid"], "vout" : utxo["vout"]} )
    outputs[to_address] = float(amount)
    #print(outputs)
    tx_to_witness = node.createrawtransaction(inputs,outputs)
    signed = node.signrawtransaction(tx_to_witness)
    return node.sendrawtransaction(signed["hex"], True)

class TempTest(TestCaseBase) :
    def set_test_params(self):
        self.num_nodes = 2

    def initialize(self) :
        pass

    # Problem: node0 sends some coins to node1 using raw transaction,
    # after the transaction is confirmed, node1 getbalance doesn't change.
    # The original test case is test_send_to_witness in segwit.py,
    # with this problem we can't know if sending raw transaction to
    # segwit address will success or not.
    def test_send_raw_transaction(self):
        print("Block count:", self.nodes[0].getblockcount(), self.nodes[1].getblockcount())
        a = self.nodes[1].getbalance();
        print("A:", a)
        
        address = self.nodes[1].getnewaddress()
        send_raw_transaction(self.nodes[0], self.nodes[0].listunspent()[0], self.nodes[1].getnewaddress(), 100)
        
        self.nodes[0].setgenerate(True, 1)
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

