Welcome to the Blocknet repository. Despite the repository name, this repo is for the Blocknet protocol. The repository for Block DX can be found [here](https://github.com/BlocknetDX/blockdx-ui).

Join the community on [Discord](https://discord.gg/2e6s7H8).

#### Blocknet
Started in 2014, Blocknet is a decentralized, community-governed, self-funded, open-source project that serves as a connector between different blockchains, markets, and communities. 

#### The Blocknet Protocol
The Blocknet protocol enables decentralized communication and exchange between different blockchains in a permissionless and trustless manner through the use of P2P atomic swaps using BIP65 and a DHT overlay network(Service Nodes). 

#### Block DX
Block DX is a completely decentralized and trustless exchange built on the Blocknet protocol that mimics a centralized exchange experience and enables traders to conduct exchanges directly from the wallets of the coins being traded. View Repo: [https://github.com/BlocknetDX/blockdx-ui]

[Contributors are welcome!](https://github.com/BlocknetDX/BlockDX/blob/master/CONTRIBUTING.md)

#### Blocknet Specifications:
- Block Time: 1 Minute
- Algo: Quark
- Difficulty: Adjusted Per Block
- Block Reward: 1.0 BLOCK (30% to stakers, 70% to Service Nodes)
- Service Node Requirement: 5000 BLOCK
- Staking Requirement: No Minimum
- Governance: Decentralized Voting/Funding With Superblocks


#### Decentralized Atomic Swap Algo Summary

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

## LICENSE

The MIT License (MIT)

Copyright (c) 2014-2018 The Blocknet Developers, see LICENSE for additional detail.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
