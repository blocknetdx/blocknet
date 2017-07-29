# BLOCKNET

![alt text](https://github.com/Aderks/blocknet/blob/master/setup-docs/pictures/block.PNG "Logo Title Text 1")

The Internet Of Blockchains


## Community-Produced FAQ document


### What Is Blocknet?
The Blocknet is a general-purpose infrastructure for inter-blockchain services. It is designed to enable the emerging “token ecosystem.” The first product build on this infrastructure is a decentralized exchange.


### What Does It  Do?
The Blocknet enables inter-blockchain services, like decentralized exchange, monetised API consumption, and p2p digital service delivery. These are core enabling features of inter-chain dapps.


### How Does It Work?
To support inter-blockchain services, the Blocknet has three core components, which work together to provide three core services.

* The core components are:

  * A blockchain router, xbridgep2ptm

  * A coin exchange protocol

  * An inter-chain data transport

* The core services are:

  * Decentralized exchange of coins

  * Inter-blockchain service delivery

  * Service monetisation
  
  
### What Is a Decentralized Exchange?
A decentralized exchange is a service enabling counterparties (which may be people or machines) to exchange one currency or token for another, without the involvement of any third party as an intermediary. 

The term “decentralized” denotes matters of control rather than the distribution of processing; the ideal of a decentralized solution is for the parties to a given interaction to be self-sovereign actors, in the sense that no third party is required to act on their behalf in order for the interaction to take place.


### How Does a Decentralized Exchange Work?
Exchanges have four core functions:

  * Capital deposits

  * Order broadcast

  * Order matching

  * The exchange of tokens

Hence, in order to be a decentralized exchange, each of these core functions must be decentralized.

The Blocknet decentralizes them in the following ways:

* Traders’ capital remains in their wallets, and is not entrusted to a third party at any stage.

* Orders are broadcast directly from trader to trader over an inter-blockchain DHT network overlay. Traders’ apps compile an order book themselves instead of relying on a central order book service.

* Orders are matched directly between traders. When one accepts another’s order, their apps communicate to set up the coin exchange process, and this is broadcast over the inter-chain network.

* The exchange of tokens is achieved without the involvement of an intermediary, in a manner that does not require counterparties to trust one another beforehand. An atomic protocol is utilised (meaning that either the exchange of both currencies takes place, or nothing takes place), which employs  OP_CHECKLOCKTIMEVERIFY to nullify transaction malleability-based attacks. More details on the protocol are available here: http://kaykurokawa.blogspot.co.za/2015/10/atomic-cross-chain-transfer-overview.html


### Why Is a Decentralized Exchange a Key Enabler Of the Token Ecosystem?
Decentralized exchange makes blockchain services intrinsically monetizable, removing the friction and high costs of traditional payment networks that have prevented the monetisation of the bulk of the API ecosystem.

Due to the decentralized exchange, consumers of a service may pay in their native token even if the service consumes a different token. In a world in which (a) there are already thousands of blockchains, and (b) blockchains bloat inexorably, and so it is advisable not to support many services per blockchain, monetising inter-chain services is both an operational necessity and an ecosystem-enabling service.


### What Coins Does the Decentralized Exchange Support?
The Blocknet was designed to maximise interoperability, and so most blockchain tokens may be integrated with no coding required.

The current integration requirements are:

* Support for OP_CHECKLOCKTIMEVERIFY

* A stock JSON RPC interface from Bitcoin Core

As a result, the Blocknet supports the majority of cryptocurrencies in existence, and no permission from anyone is required for these to be traded on the exchange.


### How Fast Is the Decentralized Exchange?
Instant.

However, note that once you have completed a trade and received coins, you will be dependent on their blockchain’s accepted confirmation time before your coins will be spendable again.

*Note: A future enhancement to the decentralized exchange may include a filter on the order book to enable traders to trade coins with less than the number of confirmations conventionally agreed upon as “safe.” This incurs a degree of risk for the benefit of supporting trading styles that require rapidly entering and exiting a position, such as scalping.*


### How Private Is the Decentralized Exchanged?
Because decentralized exchanges do not require traders to submit KYC information or divulge anything else about themselves to a third party, traders enjoy a naturally high degree of privacy.

However, for most wallets, aspects of transactions are linkable to IP addresses, so in order to obfuscate that, one might use TOR or I2P. The Blocknet’s DHT network overlay does not use IP addresses, however.

Combined with any privacy-centric coin, a decentralized exchange run over IP-obfuscating tech is a near-perfect mixing solution. For example, one may trade some coins for Zcash, sends them to a different address, and then trade back again.

### What Are the Possible Applications Of the xBridge Protocol Other Than a Decentralized Exchange?
The Blocknet is designed as infrastructure for the emerging token ecosystem. Any service or orchestrated sequence of microservices provided by dapps may be delivered over the Blocknet's infrastructure.

Using decentralized exchange, these services are intrinsically monetizable, removing the friction and high costs of traditional payment networks - friction which has prevented the monetisation of the bulk of the API ecosystem.

Due to the decentralized exchange, consumers of a service may pay in their native token even if the service consumes a different token.
 
 
### What Are the Benefits Of Running a Node? How Many Blocks Do I Need To Run One?
There are two types of node: a "service node" and a “trader node”. Service nodes do not handle or control any trader's coins. Their function is to collect and distribute trade fees. Typically a service node operator will run multiple full node wallets of whichever coins (s)he wants to support, in order to garner as many trade fees as possible. Trader nodes enable one to trade on the decentralized exchange.The amount of BLOCK currently needed to run a service node is 5,000 BLOCK. To use the exchange you will not need any BLOCK.
 
 
### Will There Be Fees For Buying/Trading On the Blocknet Exchange?
Yes, there are fees, though they are significantly lower that centralised exchanges.

The fee structure is as follows:
  * Transactions over the networks of each of your currency pairs will remain subject to their normal network fees for P2SH transactions

  * A tiny trade fee is charged in BLOCK and the software takes care of securing some BLOCK to pay the fee.


### Will A User Need BLOCK To Participate On An Exchange?
No, to use the exchange you will NOT need any BLOCK. Only the service node operators will need BLOCK in order to collect and distribute trade fees. Additionally, the service nodes do not handle or control and trader’s coins. The sole purpose of the service node is to only collect and distribute trade fees.

---

[Reddit Blocknet Q/A](https://www.reddit.com/r/theblocknet/comments/676buj/ask_anything_about_blocknet_qa/)
