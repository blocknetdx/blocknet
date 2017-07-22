## BlocknetDX
-- Decentralized Atomic Swaps

```Step1.
Initiator creates secret X, and hashes it to create H(X). Initiator also creates public private key pair (pubkey i1,i2 / privkey i2,i2). Responder creates public private key pair (pubkey r1,r2 / privkey r1,r2).

Step 2.
Initiator shares H(X) and pubkey i2 with responder. Responder shares pubkey r1 with intiator.

Step 3.
Initiator creates TxAb. TxAb can be redeemed after time T2 with privkey i1. At any time TxAb can redeemed with signature from privkey r1 and reveal of secret X. Initiator broadcasts TxAb onto the network.

Step 4.
Responder confirms TxAb. Responder creates TxBb. TxBb can be redeemed after T1 time with privkey r2. At any time TxBb can be redeemed with signature fom privkey i2 and reveal of secret X. Responder broadcasts TxBb onto the network.

Step 5.
Initiator creates TxBp which spends TxBb using privkey i2 and secret X. With the revealed secret, responder can create TxAp which spends TxAb with privkey r1 and secret X.
```
