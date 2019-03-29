Welcome to the Blocknet repository. This repo is for the Blocknet Protocol, a 2nd layer blockchain interoperability protocol that enables communication, interaction, and exchange between different blockchains. This allows for the development of multi-chain applications and blockchain microservices, creating exponentially more capabilities and possibilities for the blockchain ecosystem. The repository for Block DX, a decentralized exchange powered by the Blocknet Protocol, can be found [here](https://github.com/BlocknetDX/block-dx).

[Website](https://blocknet.co) | [API](https://api.blocknet.co) | [Documentation](https://docs.blocknet.co) | [Discord](https://discord.gg/2e6s7H8)
-------------|-------------|-------------|-------------

#### Blocknet
Started in 2014, Blocknet is a decentralized, community-governed, self-funded, open-source project that serves as a connector between different blockchains, markets, and communities. 

#### The Blocknet Protocol
The Blocknet Protocol enables decentralized communication and exchange between different blockchains in a permissionless and trustless manner through the use of the TCP/IP networking layer for communication, P2P atomic swaps using BIP65 for exchange, and a DHT overlay network (Service Nodes) to host the full nodes of compatible blockchains, host microservices, audit interactions, and perform anti-spam and anti-DOS measures for the network. 

#### Block DX
Block DX is a completely decentralized and trustless exchange built on the Blocknet Protocol that mimics a centralized exchange experience and enables traders to conduct exchanges directly from the wallets of the coins being traded. View Repo: [https://github.com/BlocknetDX/block-dx]

[Contributors are welcome!](https://github.com/BlocknetDX/blocknet/blob/master/CONTRIBUTING.md)

#### Blocknet Specifications:
|BLOCK Details 			| 					|
------------------------|--------------------
Creation Date   		| October 20th, 2014
Release Method  		| ITO, No Premine
Proof Type   			| Proof of Stake (PoS)
Algo					| Quark
Block Time 				| 60 seconds
Block Reward 			| 1.0 BLOCK <br>0.3 awarded to stakers <br>0.7 awarded to Service Nodes
Superblock 				| Up to 4,320 BLOCK
Difficulty				| Adjusted per block
Staking Requirement		| No minimum
Service Node Requirement| 5000 BLOCK
Circulation 			| [View on explorer](https://chainz.cryptoid.info/block/)
Max Supply 				| No maximum supply (PoS), but there is a maximum to [inflation](https://docs.blocknet.co/blockchain/introduction/#inflation)


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

Copyright (c) 2014-2019 The Blocknet Developers, see LICENSE for additional detail.

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
