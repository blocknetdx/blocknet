import sys
import struct

WITNESS = 1

from test_framework.script import *

class RawTransaction:
    def __init__(self, tx_data):
        self.tx_data = bytearray.fromhex(tx_data)
    def __str__(self):
        return self.tx_data.hex()
    def __repl__(self):
        return self.tx_data.hex()

transaction_data = RawTransaction(sys.argv[1])

def deserialize_int32(transaction):
    i = transaction.tx_data[:4]
    transaction.tx_data = transaction.tx_data[4:]
    return struct.unpack("<i", i)[0]

def deserialize_uint32(transaction):
    i = transaction.tx_data[:4]
    transaction.tx_data = transaction.tx_data[4:]
    return struct.unpack("<I", i)[0]

def deserialize_int64(transaction):
    i = transaction.tx_data[:8]
    transaction.tx_data = transaction.tx_data[8:]
    return struct.unpack("<q", i)[0]

def deserialize_varint(transaction):
    i = transaction.tx_data[:1]
    ch_size = struct.unpack("<B", i)[0]
    transaction.tx_data = transaction.tx_data[1:]
    if ch_size == 253:
        i = transaction.tx_data[:2]
        ch_size = struct.unpack("<H", i)[0]
        transaction.tx_data = transaction.tx_data[2:]
        assert ch_size > 253, "non-canonical ReadCompactSize"
    elif ch_size == 254:
        i = transaction.tx_data[:4]
        ch_size = struct.unpack("<I", i)[0]
        transaction.tx_data = transaction.tx_data[4:]
        assert ch_size > 0x10000, "non-canonical ReadCompactSize"
    elif ch_size == 255:
        i = transaction.tx_data[:8]
        ch_size = struct.unpack("<Q", i)[0]
        transaction.tx_data = transaction.tx_data[8:]
        assert ch_size > 0x100000000, "non-canonical ReadCompactSize"
    return ch_size

def deserialize_vector(transaction, deserialization_function):
    vec = []
    l = deserialize_varint(transaction)
    for i in range(l):
        vec.append(deserialization_function(transaction))
    return vec

def deserialize_char(transaction):
    i = transaction.tx_data[:1]
    transaction.tx_data = transaction.tx_data[1:]
    return i.hex()

def deserialize_uint256(transaction):
    i = transaction.tx_data[:32]
    transaction.tx_data = transaction.tx_data[32:]
    return i.hex()

def deserialize_outpoint(transaction):
    out_hash = deserialize_uint256(transaction)
    out_index = deserialize_uint32(transaction)
    return {'hash': out_hash, 'n': out_index}

def deserialize_script(transaction):
    script_data = ''.join(deserialize_vector(transaction, deserialize_char))
    return script_data

def deserialize_txin(transaction):
    outpoint = deserialize_outpoint(transaction)
    scriptSig = deserialize_script(transaction)
    nSequence = deserialize_uint32(transaction)
    return {'prevout': outpoint, 'scriptSig': scriptSig, 'nSequence': nSequence}

def deserialize_txout(transaction):
    value = deserialize_int64(transaction)
    scriptPubKey = deserialize_script(transaction)
    return {"value": value, "scriptPubKey": scriptPubKey}

def deserialize_scriptwitness(transaction):
    return deserialize_vector(transaction, lambda transaction: deserialize_vector(transaction, deserialize_char))

def deserialize_txinwitness(transaction):
    return deserialize_scriptwitness(transaction)

def deserialize_witness(transaction, n):
    vec = []
    for i in range(n):
        vec.append(deserialize_txinwitness(transaction))
    return vec

def deserialize_transaction(transaction):
    tx = {}
    tx["version"] = deserialize_int32(transaction)
    tx["vtxin"] = deserialize_vector(transaction, deserialize_txin)
    tx["flags"] = 0
    if len(tx["vtxin"]) == 0 and WITNESS:
        tx["flags"] = int(deserialize_char(transaction), 16)
        if tx["flags"] != 0:
            tx["vtxin"] = deserialize_vector(transaction, deserialize_txin)
            tx["vtxout"] = deserialize_vector(transaction, deserialize_txout)
    else:
        tx["vtxout"] = deserialize_vector(transaction, deserialize_txout)
    if (tx["flags"] & 1) and WITNESS:
        tx["flags"] ^= 1
        tx["witness"] = deserialize_witness(transaction, len(tx["vtxin"]))
    return tx
    

print(deserialize_transaction(transaction_data))
