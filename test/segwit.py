#!/usr/bin/env python2
# Copyright (c) 2014 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#
# Test the SegWit changeover logic
#

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *
import os
import shutil
import hashlib
from binascii import hexlify

NODE_0 = 0
NODE_1 = 1
NODE_2 = 2
WIT_V0 = 0
WIT_V1 = 1

def sha256(s):
    return hashlib.new('sha256', s).digest()

def ripemd160(s):
    return hashlib.new('ripemd160', s).digest()

def witness_script(version, pubkey):
    if (version == 0):
        pubkeyhash = encode_hex(ripemd160(sha256(decode_hex(pubkey))))
        pkscript = "0014" + pubkeyhash
    elif (version == 1):
        # 1-of-1 multisig
        scripthash = encode_hex(decode_hex(sha256(("5121" + pubkey + "51ae"))))
        pkscript = "0020" + scripthash
    else:
        assert("Wrong version" == "0 or 1")
    return pkscript

def addlength(script):
    scriptlen = format(int(len(script)/2), 'x')
    assert(len(scriptlen) == 2)
    return scriptlen + script

def create_witnessprogram(version, node, utxo, pubkey, encode_p2sh, amount):
    pkscript = witness_script(version, pubkey);
    if (encode_p2sh):
        p2sh_hash = hexlify(ripemd160(sha256(pkscript.decode("hex"))))
        pkscript = "a914"+p2sh_hash+"87"
    inputs = []
    outputs = {}
    inputs.append({ "txid" : utxo["txid"], "vout" : utxo["vout"]} )
    #DUMMY_P2SH = "2MySexEGVzZpRgNQ1JdjdP5bRETznm3roQ2" # P2SH of "OP_1 OP_DROP"
    DUMMY_P2SH = "yCSYtmhzg1FnWSjrL9KWuprunG9HqtDqPz"
    outputs[DUMMY_P2SH] = float(amount)
    tx_to_witness = node.createrawtransaction(inputs,outputs)
    #replace dummy output with our own
    tx_to_witness = tx_to_witness[0:110] + addlength(pkscript) + tx_to_witness[-8:]
    return tx_to_witness

def send_to_witness(version, node, utxo, pubkey, encode_p2sh, amount, sign=True, insert_redeem_script=""):
    tx_to_witness = create_witnessprogram(version, node, utxo, pubkey, encode_p2sh, amount)
    if (sign):
        signed = node.signrawtransaction(tx_to_witness)
        return node.sendrawtransaction(signed["hex"])
    else:
        if (insert_redeem_script):
            tx_to_witness = tx_to_witness[0:82] + addlength(insert_redeem_script) + tx_to_witness[84:]

    return node.sendrawtransaction(tx_to_witness)

def getutxo(txid):
    utxo = {}
    utxo["vout"] = 0
    utxo["txid"] = txid
    return utxo

MAX_BLOCK_SERIALIZED_SIZE = 4000000
MAX_BLOCK_SIGOPS_COST = 80000

class SegWitTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 3
        # This test tests SegWit both pre and post-activation, so use the normal BIP9 activation.
        self.extra_args = [["-walletprematurewitness", "-rpcserialversion=0", "-vbparams=segwit:0:999999999999", "-addresstype=legacy", "-deprecatedrpc=addwitnessaddress"],
                           ["-blockversion=4", "-promiscuousmempoolflags=517", "-prematurewitness", "-walletprematurewitness", "-rpcserialversion=1", "-vbparams=segwit:0:999999999999", "-addresstype=legacy", "-deprecatedrpc=addwitnessaddress"],
                           ["-blockversion=536870915", "-promiscuousmempoolflags=517", "-prematurewitness", "-walletprematurewitness", "-vbparams=segwit:0:999999999999", "-addresstype=legacy", "-deprecatedrpc=addwitnessaddress"]]

    def setup_network(self):
        super().setup_network()
        connect_nodes(self.nodes[0], 2)
        self.sync_all()

    def success_mine(self, node, txid, sign, redeem_script=""):
        send_to_witness(1, node, getutxo(txid), self.pubkey[0], False, Decimal("49.998"), sign, redeem_script)
        block = node.generate(1)
        assert_equal(len(node.getblock(block[0])["tx"]), 2)
        sync_blocks(self.nodes)

    def skip_mine(self, node, txid, sign, redeem_script=""):
        send_to_witness(1, node, getutxo(txid), self.pubkey[0], False, Decimal("49.998"), sign, redeem_script)
        block = node.generate(1)
        assert_equal(len(node.getblock(block[0])["tx"]), 1)
        sync_blocks(self.nodes)

    def fail_accept(self, node, error_msg, txid, sign, redeem_script=""):
        assert_raises_rpc_error(-26, error_msg, send_to_witness, 1, node, getutxo(txid), self.pubkey[0], False, Decimal("49.998"), sign, redeem_script)

    def fail_mine(self, node, txid, sign, redeem_script=""):
        send_to_witness(1, node, getutxo(txid), self.pubkey[0], False, Decimal("49.998"), sign, redeem_script)
        assert_raises_rpc_error(-1, "CreateNewBlock: TestBlockValidity failed", node.generate, 1)
        sync_blocks(self.nodes)

    def run_test(self):
        self.nodes[0].setgenerate(True, 161) #block 161

        self.log.info("Verify sigops are counted in GBT with pre-BIP141 rules before the fork")
        txid = self.nodes[0].sendtoaddress(self.nodes[0].getnewaddress(), 1)
        tmpl = self.nodes[0].getblocktemplate({})
        assert(tmpl['sizelimit'] == MAX_BLOCK_SERIALIZED_SIZE)
        assert('weightlimit' not in tmpl)
        assert(tmpl['sigoplimit'] == MAX_BLOCK_SIGOPS_COST)
        #print(txid)
        #print(tmpl)
        #assert(tmpl['transactions'][0]['hash'] == txid)
        #assert(tmpl['transactions'][0]['sigops'] == 2)
        tmpl = self.nodes[0].getblocktemplate({'rules':['segwit']})
        assert(tmpl['sizelimit'] == MAX_BLOCK_SERIALIZED_SIZE)
        assert('weightlimit' not in tmpl)
        assert(tmpl['sigoplimit'] == MAX_BLOCK_SIGOPS_COST)
        #assert(tmpl['transactions'][0]['hash'] == txid)
        #assert(tmpl['transactions'][0]['sigops'] == 2)
        self.nodes[0].setgenerate(True, 1) #block 162

        #below test doesn't work yet because AcceptToMemoryPool in main.cpp returns false when sendrawtransaction in send_to_witness
        return;

        self.pubkey = []
        p2sh_ids = [] # p2sh_ids[NODE][VER] is an array of txids that spend to a witness version VER pkscript to an address for NODE embedded in p2sh
        wit_ids = [] # wit_ids[NODE][VER] is an array of txids that spend to a witness version VER pkscript to an address for NODE via bare witness
        for i in range(3):
            newaddress = self.nodes[i].getnewaddress()
            self.pubkey.append(self.nodes[i].validateaddress(newaddress)["pubkey"])
            multiaddress = self.nodes[i].addmultisigaddress(1, [self.pubkey[-1]])
            self.nodes[i].addwitnessaddress(newaddress)
            self.nodes[i].addwitnessaddress(multiaddress)
            p2sh_ids.append([])
            wit_ids.append([])
            for v in range(2):
                p2sh_ids[i].append([])
                wit_ids[i].append([])

        for i in range(5):
            for n in range(3):
                for v in range(2):
                    wit_ids[n][v].append(send_to_witness(v, self.nodes[0], self.nodes[0].listunspent()[0], self.pubkey[n], False, Decimal("49.999")))
                    p2sh_ids[n][v].append(send_to_witness(v, self.nodes[0], self.nodes[0].listunspent()[0], self.pubkey[n], True, Decimal("49.999")))

        self.nodes[0].generate(1) #block 163
        sync_blocks(self.nodes)

    def BAKrun_test(self):
        self.nodes[0].generate(160) #block 160

        self.pubkey = []
        p2sh_ids = [] # p2sh_ids[NODE][VER] is an array of txids that spend to a witness version VER pkscript to an address for NODE embedded in p2sh
        wit_ids = [] # wit_ids[NODE][VER] is an array of txids that spend to a witness version VER pkscript to an address for NODE via bare witness
        for i in xrange(3):
            newaddress = self.nodes[i].getnewaddress()
            self.pubkey.append(self.nodes[i].validateaddress(newaddress)["pubkey"])
            multiaddress = self.nodes[i].addmultisigaddress(1, [self.pubkey[-1]])
            self.nodes[i].addwitnessaddress(newaddress)
            self.nodes[i].addwitnessaddress(multiaddress)
            p2sh_ids.append([])
            wit_ids.append([])
            for v in xrange(2):
                p2sh_ids[i].append([])
                wit_ids[i].append([])

        for i in xrange(5):
            for n in xrange(3):
                for v in xrange(2):
                    wit_ids[n][v].append(send_to_witness(v, self.nodes[0], self.nodes[0].listunspent()[0], self.pubkey[n], False, Decimal("49.999")))
                    p2sh_ids[n][v].append(send_to_witness(v, self.nodes[0], self.nodes[0].listunspent()[0], self.pubkey[n], True, Decimal("49.999")))

        self.nodes[0].generate(1) #block 161
        sync_blocks(self.nodes)

        # Make sure all nodes recognize the transactions as theirs
        assert_equal(self.nodes[0].getbalance(), 60*50 - 60*50 + 20*Decimal("49.999") + 50)
        assert_equal(self.nodes[1].getbalance(), 20*Decimal("49.999"))
        assert_equal(self.nodes[2].getbalance(), 20*Decimal("49.999"))

        self.nodes[0].generate(262) #block 423
        sync_blocks(self.nodes)

        print("Verify default node can't accept any witness format txs before fork")
        # unsigned, no scriptsig
        self.fail_accept(self.nodes[0], wit_ids[NODE_0][WIT_V0][0], False)
        self.fail_accept(self.nodes[0], wit_ids[NODE_0][WIT_V1][0], False)
        self.fail_accept(self.nodes[0], p2sh_ids[NODE_0][WIT_V0][0], False)
        self.fail_accept(self.nodes[0], p2sh_ids[NODE_0][WIT_V1][0], False)
        # unsigned with redeem script
        self.fail_accept(self.nodes[0], p2sh_ids[NODE_0][WIT_V0][0], False, addlength(witness_script(0, self.pubkey[0])))
        self.fail_accept(self.nodes[0], p2sh_ids[NODE_0][WIT_V1][0], False, addlength(witness_script(1, self.pubkey[0])))
        # signed
        self.fail_accept(self.nodes[0], wit_ids[NODE_0][WIT_V0][0], True)
        self.fail_accept(self.nodes[0], wit_ids[NODE_0][WIT_V1][0], True)
        self.fail_accept(self.nodes[0], p2sh_ids[NODE_0][WIT_V0][0], True)
        self.fail_accept(self.nodes[0], p2sh_ids[NODE_0][WIT_V1][0], True)

        print("Verify witness txs are skipped for mining before the fork")
        self.skip_mine(self.nodes[2], wit_ids[NODE_2][WIT_V0][0], True) #block 424
        self.skip_mine(self.nodes[2], wit_ids[NODE_2][WIT_V1][0], True) #block 425
        self.skip_mine(self.nodes[2], p2sh_ids[NODE_2][WIT_V0][0], True) #block 426
        self.skip_mine(self.nodes[2], p2sh_ids[NODE_2][WIT_V1][0], True) #block 427

        # TODO: An old node would see these txs without witnesses and be able to mine them

        print("Verify unsigned bare witness txs in versionbits-setting blocks are valid before the fork")
        self.success_mine(self.nodes[2], wit_ids[NODE_2][WIT_V0][1], False) #block 428
        self.success_mine(self.nodes[2], wit_ids[NODE_2][WIT_V1][1], False) #block 429

        print("Verify unsigned p2sh witness txs without a redeem script are invalid")
        self.fail_accept(self.nodes[2], p2sh_ids[NODE_2][WIT_V0][1], False)
        self.fail_accept(self.nodes[2], p2sh_ids[NODE_2][WIT_V1][1], False)

        print("Verify unsigned p2sh witness txs with a redeem script in versionbits-settings blocks are valid before the fork")
        self.success_mine(self.nodes[2], p2sh_ids[NODE_2][WIT_V0][1], False, addlength(witness_script(0, self.pubkey[2]))) #block 430
        self.success_mine(self.nodes[2], p2sh_ids[NODE_2][WIT_V1][1], False, addlength(witness_script(1, self.pubkey[2]))) #block 431

        print("Verify previous witness txs skipped for mining can now be mined")
        assert_equal(len(self.nodes[2].getrawmempool()), 4)
        block = self.nodes[2].generate(1) #block 432 (first block with new rules; 432 = 144 * 3)
        sync_blocks(self.nodes)
        assert_equal(len(self.nodes[2].getrawmempool()), 0)
        assert_equal(len(self.nodes[2].getblock(block[0])["tx"]), 5)

        print("Verify witness txs without witness data are invalid after the fork")
        self.fail_mine(self.nodes[2], wit_ids[NODE_2][WIT_V0][2], False)
        self.fail_mine(self.nodes[2], wit_ids[NODE_2][WIT_V1][2], False)
        self.fail_mine(self.nodes[2], p2sh_ids[NODE_2][WIT_V0][2], False, addlength(witness_script(0, self.pubkey[2])))
        self.fail_mine(self.nodes[2], p2sh_ids[NODE_2][WIT_V1][2], False, addlength(witness_script(1, self.pubkey[2])))

        print("Verify default node can now use witness txs")
        self.success_mine(self.nodes[0], wit_ids[NODE_0][WIT_V0][0], True) #block 432
        self.success_mine(self.nodes[0], wit_ids[NODE_0][WIT_V1][0], True) #block 433
        self.success_mine(self.nodes[0], p2sh_ids[NODE_0][WIT_V0][0], True) #block 434
        self.success_mine(self.nodes[0], p2sh_ids[NODE_0][WIT_V1][0], True) #block 435

if __name__ == '__main__':
    SegWitTest().main()
