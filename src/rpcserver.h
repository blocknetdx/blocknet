/** \file rpcserver.h
 * Functions exposed for RPC.
 *
 * Copyright (c) 2010 Satoshi Nakamoto
 * Copyright (c) 2009-2014 The Bitcoin developers
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#ifndef BITCOIN_RPCSERVER_H
#define BITCOIN_RPCSERVER_H

#include "amount.h"
#include "rpcprotocol.h"
#include "uint256.h"

#include <list>
#include <map>
#include <stdint.h>
#include <string>

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_utils.h"
#include "json/json_spirit_writer_template.h"

class CBlockIndex;
class CNetAddr;

class AcceptedConnection
{
public:
    virtual ~AcceptedConnection() {}

    virtual std::iostream & stream() = 0;
    virtual std::string peer_address_to_string() const = 0;
    virtual void start() = 0;
    virtual void close() = 0;
    virtual bool is_closed() = 0;
};

/** Start RPC threads */
void StartRPCThreads();
/**
 * Alternative to StartRPCThreads for the GUI, when no server is
 * used. The RPC thread in this case is only used to handle timeouts.
 * If real RPC threads have already been started this is a no-op.
 */
void StartDummyRPCThread();
/** Stop RPC threads */
void StopRPCThreads();
/** Query whether RPC is running */
bool IsRPCRunning();

/**
 * Set the RPC warmup status.  When this is done, all RPC calls will error out
 * immediately with RPC_IN_WARMUP.
 */
void SetRPCWarmupStatus(const std::string& newStatus);
/* Mark warmup as done.  RPC calls will be processed from now on.  */
void SetRPCWarmupFinished();

/* returns the current warmup state.  */
bool RPCIsInWarmup(std::string* statusOut);

/**
 * Type-check arguments; throws JSONRPCError if wrong type given. Does not check that
 * the right number of arguments are passed, just that any passed are the correct type.
 * Use like:  RPCTypeCheck(params, boost::assign::list_of(str_type)(int_type)(obj_type));
 */
void RPCTypeCheck(const json_spirit::Array& params,
    const std::list<json_spirit::Value_type>& typesExpected,
    bool fAllowNull = false);
/**
 * Check for expected keys/value types in an Object.
 * Use like: RPCTypeCheck(object, boost::assign::map_list_of("name", str_type)("value", int_type));
 */
void RPCTypeCheck(const json_spirit::Object& o,
    const std::map<std::string, json_spirit::Value_type>& typesExpected,
    bool fAllowNull = false);

/**
 * Run func nSeconds from now. Uses boost deadline timers.
 * Overrides previous timer <name> (if any).
 */
void RPCRunLater(const std::string& name, boost::function<void(void)> func, int64_t nSeconds);

//! Convert boost::asio address to CNetAddr
extern CNetAddr BoostAsioToCNetAddr(boost::asio::ip::address address);

typedef json_spirit::Value (*rpcfn_type)(const json_spirit::Array& params, bool fHelp);

class CRPCCommand
{
public:
    std::string category;
    std::string name;
    rpcfn_type actor;
    bool okSafeMode;
    bool threadSafe;
    bool reqWallet;
};

/**
 * BlocknetDX RPC command dispatcher.
 */
class CRPCTable
{
private:
    std::map<std::string, const CRPCCommand*> mapCommands;

public:
    CRPCTable();
    const CRPCCommand* operator[](std::string name) const;
    std::string help(std::string name) const;

    /**
     * Execute a method.
     * @param method   Method to execute
     * @param params   Array of arguments (JSON objects)
     * @returns Result of the call.
     * @throws an exception (json_spirit::Value) when an error happens.
     */
    json_spirit::Value execute(const std::string& method, const json_spirit::Array& params) const;
};

extern const CRPCTable tableRPC;

/**
 * Utilities: convert hex-encoded Values
 * (throws error if not hex).
 */
extern uint256 ParseHashV(const json_spirit::Value& v, std::string strName);
extern uint256 ParseHashO(const json_spirit::Object& o, std::string strKey);
extern std::vector<unsigned char> ParseHexV(const json_spirit::Value& v, std::string strName);
extern std::vector<unsigned char> ParseHexO(const json_spirit::Object& o, std::string strKey);

extern void InitRPCMining();
extern void ShutdownRPCMining();

extern int64_t nWalletUnlockTime;
extern CAmount AmountFromValue(const json_spirit::Value& value);
extern json_spirit::Value ValueFromAmount(const CAmount& amount);
extern double GetDifficulty(const CBlockIndex* blockindex = NULL);
extern std::string HelpRequiringPassphrase();
extern std::string HelpExampleCli(std::string methodname, std::string args);
extern std::string HelpExampleRpc(std::string methodname, std::string args);

extern void EnsureWalletIsUnlocked();

extern json_spirit::Value getconnectioncount(const json_spirit::Array& params, bool fHelp); // in rpcnet.cpp
extern json_spirit::Value getpeerinfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value ping(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value addnode(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getaddednodeinfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getnettotals(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value sendserviceping(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value disconnectpeer(const json_spirit::Array& params, bool fHelp);

extern json_spirit::Value dumpprivkey(const json_spirit::Array& params, bool fHelp); // in rpcdump.cpp
extern json_spirit::Value importprivkey(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value importaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value dumpwallet(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value importwallet(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value bip38encrypt(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value bip38decrypt(const json_spirit::Array& params, bool fHelp);

extern json_spirit::Value setstakesplitthreshold(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getstakesplitthreshold(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getgenerate(const json_spirit::Array& params, bool fHelp); // in rpcmining.cpp
extern json_spirit::Value setgenerate(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getnetworkhashps(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value gethashespersec(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getmininginfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value prioritisetransaction(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getblocktemplate(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value submitblock(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value estimatefee(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value estimatepriority(const json_spirit::Array& params, bool fHelp);

extern json_spirit::Value getnewaddress(const json_spirit::Array& params, bool fHelp); // in rpcwallet.cpp
extern json_spirit::Value getaccountaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getrawchangeaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value setaccount(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getaccount(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getaddressesbyaccount(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value sendtoaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value sendtoaddressix(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value signmessage(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value verifymessage(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getreceivedbyaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getreceivedbyaccount(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getbalance(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getunconfirmedbalance(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value movecmd(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value sendfrom(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value sendmany(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value addmultisigaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value createmultisig(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listreceivedbyaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listreceivedbyaccount(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listtransactions(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listaddressgroupings(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listaccounts(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listsinceblock(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value gettransaction(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value backupwallet(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value keypoolrefill(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value walletpassphrase(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value walletpassphrasechange(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value walletlock(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value encryptwallet(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value validateaddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getinfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getwalletinfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getblockchaininfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getnetworkinfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value setmocktime(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value reservebalance(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value multisend(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value autocombinerewards(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getstakingstatus(const json_spirit::Array& params, bool fHelp);

extern json_spirit::Value getrawtransaction(const json_spirit::Array& params, bool fHelp); // in rcprawtransaction.cpp
extern json_spirit::Value listunspent(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value lockunspent(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value listlockunspent(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value createrawtransaction(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value fundrawtransaction(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value decoderawtransaction(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value decodescript(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value signrawtransaction(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value sendrawtransaction(const json_spirit::Array& params, bool fHelp);

extern json_spirit::Value getblockcount(const json_spirit::Array& params, bool fHelp); // in rpcblockchain.cpp
extern json_spirit::Value getbestblockhash(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getdifficulty(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value settxfee(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getmempoolinfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getrawmempool(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getblockhash(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getblock(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getblockheader(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value gettxoutsetinfo(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value gettxout(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value verifychain(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value getchaintips(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value invalidateblock(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value reconsiderblock(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value obfuscation(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value spork(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value servicenode(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value servicenodelist(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value mnbudget(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value mnbudgetvoteraw(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value mnfinalbudget(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value mnsync(const json_spirit::Array& params, bool fHelp);

/** \defgroup xBridgeAPI xBridge API
 * @brief XBridge functions exposed to RPC
 *  @{
 */

/** @brief Hot loads xbridge.conf while the dx is running
  * @param params The list of input params should be empty
  * @param fHelp If is true then an exception with parameter description message will be thrown
  * @return true if successful otherwise returns false
  * * Example:<br>
  * \verbatim
    dxLoadXBridgeConf
￼
    true
  * \endverbatim
  */
extern json_spirit::Value dxLoadXBridgeConf(const json_spirit::Array& params, bool fHelp);

/** \defgroup xBridgeAPI xBridge API
 * @brief XBridge functions exposed to RPC
 *  @{
 */

/** @brief Returns the list of open and pending transactions
  * @param params The list of input params - should be empty
  * @param fHelp If is true then an exception with parameter description message will be thrown
  * @return The list of open and pending transactions as JSON value. Open transactions go first.
  * * Example:<br>
  * \verbatim
    dxGetOrders
￼
    [
        {
            "id" : "1632417312d5ea676abb88b8fb48ace1a11e9b1a937fc24ff79296d9d2963b32",
            "from" : "BLOCK",
            "fromAddress" : "",
            "fromAmount" : 0.01000000000000000,
            "to" : "SYS",
            "toAddress" : "",
            "toAmount" : 0.50000000000000000,
            "state" : "Open"
        },
        {
            "id" : "6be548bc46a3dcc69b6d56529948f7e679dd96657f85f5870a017e005caa050a",
            "from" : "LTC",
            "fromAddress" : "",
            "fromAmount" : 0.00010000000000000,
            "to" : "SYS",
            "toAddress" : "",
            "toAmount" : 0.00010000000000000,
            "state" : "Open"
        }
    ]
  * \endverbatim
  */

extern json_spirit::Value dxGetOrders(const json_spirit::Array& params, bool fHelp);

/**
 * @brief Returns the list of historical(closed) transactions
 * @param params The list of input params:<br>
 * params[0] : optional parameter, if it's specified and equals to "ALL" then all transactions will be
 * returned, not only successfully completed ones
 * @param fHelp If is true then an exception with parameter description message will be thrown
 * @return The list of historical transaction  as a JSON value
 * * Example:<br>
 * \verbatim
    dxGetOrderFills ALL
￼
    [
        {
            "id" : "1632417312d5ea676abb88b8fb48ace1a11e9b1a937fc24ff79296d9d2963b32",
            "from" : "BLOCK",
            "fromAddress" : "",
            "fromAmount" : 0.01000000000000000,
            "to" : "SYS",
            "toAddress" : "",
            "toAmount" : 0.50000000000000000,
            "state" : "Finished"
        },
        {
            "id" : "6be548bc46a3dcc69b6d56529948f7e679dd96657f85f5870a017e005caa050a",
            "from" : "LTC",
            "fromAddress" : "",
            "fromAmount" : 0.00010000000000000,
            "to" : "SYS",
            "toAddress" : "",
            "toAmount" : 0.00010000000000000,
            "state" : "Finished"
        }
        {
            "id" : "6be54829948f7e679dd96657f8bc46a3dcc69b6d5655f5870a017e005caa050a",
            "from" : "LTC",
            "fromAddress" : "",
            "fromAmount" : 0.00010000000000000,
            "to" : "SYS",
            "toAddress" : "",
            "toAmount" : 0.00010000000000000,
            "state" : "Cancelled"
        }
    ]
 * \endverbatim
 */
extern json_spirit::Value dxGetOrderFills(const json_spirit::Array& params, bool fHelp);

/**
 * @brief Returns the detailed description of given a transaction
 * @param params The list of input params:<br>
 * params[0] : transaction id
 * @param fHelp If is true then an exception with parameter description message will be thrown
 * @return The detailed description of given transaction as a JSON value
 * * Example:<br>
 * \verbatim
    dxGetOrder 91d0ea83edc79b9a2041c51d08037cff87c181efb311a095dfdd4edbcc7993a9
￼
    [
        {
            "id" : "91d0ea83edc79b9a2041c51d08037cff87c181efb311a095dfdd4edbcc7993a9",
            "from" : "LTC",
            "fromAddress" : "",
            "fromAmount" : 0.01000000000000000,
            "to" : "BLOCK",
            "toAddress" : "",
            "toAmount" : 0.05500000000000000,
            "state" : "Open"
        }
    ]
 * \endverbatim
 */
extern json_spirit::Value dxGetOrder(const json_spirit::Array& params, bool fHelp);


/**
 * @brief Returns the list of the currencies supported by the wallet
 * @param params The list of input params, should be empty
 * @param fHelp If is true then an exception with parameter description message will be thrown
 * @return The list of available currencies as a JSON value
 * * Example:<br>
 * \verbatim
    {
        "DCR" : "",
        "DEC" : "",
        "DOGE" : "",
        "LTC" : "",
        "SYS" : ""
    }
 * \endverbatim
 */
extern json_spirit::Value dxGetLocalTokens(const json_spirit::Array& params, bool fHelp);

/**
 * @brief Returns the list of the currencies supported bt the network
 * @param params The list of input params, should be empty
 * @param fHelp If is true then an exception with parameter description message will be thrown
 * @return The list of available currencies as a JSON value
 * * Example:<br>
 * \verbatim
    {
        "DCR" : "",
        "DEC" : "",
        "DOGE" : "",
        "LTC" : "",
        "SYS" : ""
    }
 * \endverbatim
 */
extern json_spirit::Value dxGetNetworkTokens(const json_spirit::Array& params, bool fHelp);

/**
 * @brief Creates a new transaction
 * @param params The list of input params:<br>
 * params[0] : sending address<br>
 * params[1] : currency being sent<br>
 * params[2] : amount being sent<br>
 * params[3] : receiving address<br>
 * params[4] : currency being received<br>
 * params[5] : amount being received<br>
 * params[6] : type of opetation<br>
 * @param fHelp If is true then an exception with parameter description message will be thrown
 * @return The transaction created, as a JSON value
 * * Example:<br>
 * \verbatim
 *
    dxMakeOrder  1NDqZ7piDqyDhNveWS48kDSwPdyJLEEcCp SYS 1.3 LRuXAU2fdSU7imXzk8cTy2k3heMK5vTuQ4 LTC 0.13 exact
    {
        "from" : "1NDqZ7piDqyDhNveWS48kDSwPdyJLEEcCp",
        "fromCurrency" : "SYS",
        "fromAmount" : 1.30000000000000004,
        "to" : "LRuXAU2fdSU7imXzk8cTy2k3heMK5vTuQ4",
        "toCurrency" : "LTC",
        "toAmount" : 0.13000000000000000
    }
 * \endverbatim
 */
extern json_spirit::Value dxMakeOrder(const json_spirit::Array& params, bool fHelp);

/**
 * @brief Accepts given transaction
 * @param params The list of input params:<br>
 * params[0] : transaction id<br>
 * params[1] : sending address<br>
 * params[2] : receiving address<br>
 * @param fHelp If is true then an exception with parameter description message will be thrown
 * @return The status of the operation
 * * Example:<br>
 * \verbatim￼
    dxAcceptTransaction 6be548bc46a3dcc69b6d56529948f7e679dd96657f85f5870a017e005caa050a 1NDqZ7piDqyDhNveWS48kDSwPdyJLEEcCp LRuXAU2fdSU7imXzk8cTy2k3heMK5vTuQ4
    {
        "id" : "6be548bc46a3dcc69b6d56529948f7e679dd96657f85f5870a017e005caa050a",
        "from" : "1NDqZ7piDqyDhNveWS48kDSwPdyJLEEcCp",
        "to" : "LRuXAU2fdSU7imXzk8cTy2k3heMK5vTuQ4"
    }
 * \endverbatim
 */
extern json_spirit::Value dxTakeOrder(const json_spirit::Array& params, bool fHelp);

/**
 * @brief Cancels given order
 * @param params The list of input params:<br>
 * params[0] : transaction id<br>
 * @param fHelp If is true then an exception with parameter description message will be thrown
 * @return The status of the operation
 * * Example:<br>
 * \verbatim ￼
    dxCancelOrder 6be548bc46a3dcc69b6d56529948f7e679dd96657f85f5870a017e005caa050a
    {

        "id" : "6be548bc46a3dcc69b6d56529948f7e679dd96657f85f5870a017e005caa050a"
        "maker" : "SYS"
        "maker_size" : "0.1"
        "maker_address" : "1NDqZ7piDqyDhNveWS48kDSwPdyJLEEcCp"
        "taker" : "LTC"
        "taker_size" : "0.01"
        "taker_address" : "LRuXAU2fdSU7imXzk8cTy2k3heMK5vTuQ4"
        "updated_at" : "2018-03-01-14:18:31.zzz"
        "created_at" : "2018-03-01-13:28:31.zzz"
        "status" : "cancelled"
}
 * \endverbatim
 */
extern json_spirit::Value dxCancelOrder(const json_spirit::Array& params, bool fHelp);

/**
 * @brief Returns trading history as a 'price chart'
 * @param params The list of input params:<br>
 * params[0] : currency sent<br>
 * params[1] : currency received<br>
 * params[2] : start time, Unix time<br>
 * params[3] : end time, Unix time<br>
 * @param fHelp If is true then an exception with parameter description message will be thrown
 * @return The list of completed transactions as 'price chart' points
 * * Example:<br>
 * \verbatim
  [
   {
        "bids" : [
            [
                1.00000000000000000,
                0.00200000000000000
            ],
            [
                1.00000000000000000,
                0.00100000000000000
            ]
         ],
        "asks" : [
        ]
    }
  ]
 * \endverbatim
 */
extern json_spirit::Value dxGetOrderHistory(const json_spirit::Array& params, bool fHelp);

/**
 * @brief Returns transactions list in a form of 'order book'
 * @param params The list of input params:<br>
 * params[0] - detail level:<br>
 * 1 : The best ask and the best bid for all the time are returned<br>
 * 2 : Top <num> asks and bids are returned in separate lists, see param[3]<br>
 * 3 : All asks and bids are returned<br>
 * params[1] : currency sent<br>
 * params[2] : currency received<br>
 * params[3] : optional, the maximum number of orders to return, applicable only to detail level 2, the default value is 50<br>
 * @param fHelp If is true then an exception with parameter description message will be thrown
 * @return The list of transactions as 'order book' records<br>
 * Example:<br>
 * \verbatim
  [
   {
        "bids" : [
            [
                1.00000000000000000,
                0.00200000000000000
            ],
            [
                1.00000000000000000,
                0.00100000000000000
            ]
         ],
        "asks" : [
        ]
    }
  ]
 * \endverbatim
 */
extern json_spirit::Value dxGetOrderBook(const json_spirit::Array& params, bool fHelp);


/**
 * @brief Returns the list of the orders created by the user
 * @param params The list of input params, should be empty
 * @param fHelp If is true then an exception with parameter description message will be thrown
 * @return The list of the orders created by the user
 * * Example:<br>
 * \verbatim
  [
    {
      "id": "2cd2a2ac-e6ff-4beb-9b45-d460bf83a092",
      "maker": "SYS",
      "maker_size": "0.100",
      "maker_address": "yFMXXUJF7pSKegHTkTYMjfNxyUGVt1uCrL",
      "taker": "LTC",
      "taker_size": "0.01",
      "taker_address": "yGDmuy8m1Li4ShNe7kGYusACw4oyiGiK5b",
      "updated_at": "2018-01-15T18:25:05.12345Z",
      "created_at": "2018-01-15T18:15:30.12345Z",
      "status": "filled"
    }
  ]
 * \endverbatim
 */
extern json_spirit::Value dxGetMyOrders(const json_spirit::Array& params, bool fHelp);

/**
 * @brief Returns locked utxo list
 * @param params The list of input params:<br>
 * params[0] : order id
 * @param fHelp If is true then an exception with parameter description message will be thrown
 * @return The list of locked utxo for an order
 * * Example:<br>
 * \verbatim
    dxGetLockedUtxos 91d0ea83edc79b9a2041c51d08037cff87c181efb311a095dfdd4edbcc7993a9
    [
        {
            "id" : "91d0ea83edc79b9a2041c51d08037cff87c181efb311a095dfdd4edbcc7993a9",
            "LTC" :
            [
                6be548bc46a3dcc69b6d56529948f7e679dd96657f85f5870a017e005caa050a,
                6be548bc46a3dcc69b6d56529948f7e679dd96657f85f5870a017e005caa050a,
                6be548bc46a3dcc69b6d56529948f7e679dd96657f85f5870a017e005caa050a
            ]
        }
    ]
 * \endverbatim
 */
extern json_spirit::Value dxGetLockedUtxos(const json_spirit::Array& params, bool fHelp);

/**
 * @brief dxGetTokenBalances
 * @param params The list of input params, should be empty
 * @param fHelp If is true then an exception with parameter description message will be thrown
 * @return list of currences with balance
 */
extern json_spirit::Value  dxGetTokenBalances(const json_spirit::Array& params, bool fHelp);

/**
 * @brief Flush cancelled orders older than given milliseconds
 * @param params The list of input params:<br>
 * params[0] : optional parameter, the minimum age of order in milliseconds, the default is to flush all cancelled orders (0 milliseconds) <br>
 * @param fHelp If is true then an exception with parameter description message will be thrown
 * @return The list of flushed orders
 * * Example:<br>
 * \verbatim
    dxFlushCancelledOrders 2500
    {
        "ageMillis" : 2500,
        "now" : "20180619T042505.249373",
        "durationMicrosec" : 7,
        "flushedOrders" : [
            {
                "id" : "1632417312d5ea676abb88b8fb48ace1a11e9b1a937fc24ff79296d9d2963b32",
                "txtime" : "20180619T042309.149572",
                "use_count" : 1
            }
        ]
    }
 * \endverbatim
 */
extern json_spirit::Value dxFlushCancelledOrders(const json_spirit::Array& params, bool fHelp);
/** @} */

/**
 * @brief gettradingdata
 * @param params
 * @param fHelp
 * @return
 */
extern json_spirit::Value gettradingdata(const json_spirit::Array & params, bool fHelp);

/** @} */


/** \defgroup xRouterAPI xRouter API
 * @brief XBridge functions exposed to RPC
 *  @{
 */

/** @brief Look up a block in the specified blockchain.
  * @param params The list of input params - should be empty
  * @param fHelp If is true then an exception with parameter description message will be thrown
  * @return The list of open and pending transactions as JSON value. Open transactions go first.
  * * Example:<br>
  * \verbatim
    xrGetBlock
￼
    [
        {
        },
        {
        }
    ]
  * \endverbatim
  */

extern json_spirit::Value xrGetBlockCount(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value xrGetBlockHash(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value xrGetBlock(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value xrGetTransaction(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value xrGetBlocks(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value xrGetTransactions(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value xrGetBalance(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value xrGetBalanceUpdate(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value xrGetTransactionsBloomFilter(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value xrGenerateBloomFilter(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value xrGetReply(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value xrSendTransaction(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value xrCustomCall(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value xrUpdateConfigs(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value xrShowConfigs(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value xrReloadConfigs(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value xrStatus(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value xrRegisterDomain(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value xrQueryDomain(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value xrCreateDepositAddress(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value xrTimeToBlockNumber(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value xrOpenConnections(const json_spirit::Array& params, bool fHelp);
extern json_spirit::Value xrTest(const json_spirit::Array& params, bool fHelp);

/** @} */

// in rest.cpp
extern bool HTTPReq_REST(AcceptedConnection* conn,
    std::string& strURI,
    std::map<std::string, std::string>& mapHeaders,
    bool fRun);

#endif // BITCOIN_RPCSERVER_H
