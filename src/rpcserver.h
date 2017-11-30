// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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

    virtual std::iostream& stream() = 0;
    virtual std::string peer_address_to_string() const = 0;
    virtual void close() = 0;
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

//dx API

/** @brief Returns the list of open and pending transactions
  * @param params The list of input params - should be empty
  * @param fHelp if true, and exception with parameter description message will be thrown
  * @return The list of open and pending transactions as JSON value. Open transactions go first.
  */
extern json_spirit::Value dxGetTransactionList(const json_spirit::Array& params, bool fHelp);

/**
 * @brief dxGetTransactionsHistoryList Returns the list of historical(closed) transactions
 * @param params The list of input params:
 * params[0] - optional parameter, if it's specified and equals to "ALL" then all transactions will be
 * returned, not only successfully completed ones
 * @param fHelp if true, and exception with parameter description message will be thrown
 * @return The list of historical transaction  as a JSON value
 */
extern json_spirit::Value dxGetTransactionsHistoryList(const json_spirit::Array& params, bool fHelp);

/**
 * @brief dxGetTransactionInfo Returns the detailed description of given a transaction
 * @param params The list of input params:
 * params[0] - transaction id
 * @param fHelp if true, and exception with parameter description message will be thrown
 * @return The detailed description of given transaction as a JSON value
 */
extern json_spirit::Value dxGetTransactionInfo(const json_spirit::Array& params, bool fHelp);

/**
 * @brief dxGetCurrencyList Returns the list of available currencies
 * @param params The list of input params - should be empty
 * @param fHelp if true, and exception with parameter description message will be thrown
 * @return The list of available currencies as a JSON value
 */
extern json_spirit::Value dxGetCurrencyList(const json_spirit::Array& params, bool fHelp);

/**
 * @brief dxCreateTransaction Creates a new transaction
 * @param params The list of input params
 * params[0] - sending address
 * params[1] - currency being sent
 * params[2] - amount being sent
 * params[3] - receiving address
 * params[4] - currency being received
 * params[5] - amount being received
 * @param fHelp if true, and exception with parameter description message will be thrown
 * @return The transaction created, as a JSON value
 */
extern json_spirit::Value dxCreateTransaction(const json_spirit::Array& params, bool fHelp);

/**
 * @brief dxAcceptTransaction Accepts given transaction
 * @param params The list of input params:
 * params[0] - transaction id
 * params[1] - sending address
 * params[2] - receiving address
 * @param fHelp if true, and exception with parameter description message will be thrown
 * @return The status of the operation
 */
extern json_spirit::Value dxAcceptTransaction(const json_spirit::Array& params, bool fHelp);

/**
 * @brief dxCancelTransaction Cancels given transaction
 * @param params The list of input params:
 * params[0] - transaction id
 * @param fHelp if true, and exception with parameter description message will be thrown
 * @return The status of the operation
 */
extern json_spirit::Value dxCancelTransaction(const json_spirit::Array& params, bool fHelp);

/**
 * @brief dxGetTradeHistory Returns trading history as a 'price chart'
 * @param params The list of input params
 * params[0] - currency sent
 * params[1] - currency received
 * params[2] - start time, Unix time
 * params[3] - end time, Unix time
 * @param fHelp if true, and exception with parameter description message will be thrown
 * @return The list of completed transactions as 'price chart' points
 */
extern json_spirit::Value dxGetTradeHistory(const json_spirit::Array& params, bool fHelp);

/**
 * @brief dxGetOrderBook Returns transactions list in a form of 'order book'
 * @param params The list of input params:
 * params[0] - detail level:
 * 1 : The best ask and the best bid for all the time are returned
 * 2 : Top <num> asks and bids are returned in separate lists, see param[3]
 * 3 : All asks and bids are returned
 * params[1] - currency sent
 * params[2] - currency received
 * params[3] - optional, the maximum number of orders to return, applicable only to detail level 2, the default value is 50
 * @param fHelp if true, and exception with parameter description message will be thrown
 * @return The list of transactions as 'order book' records
 * Example:
 * [
 *  {
 *       "bids" : [
 *           [
 *               1.00000000000000000,
 *               0.00200000000000000
 *           ],
 *           [
 *               1.00000000000000000,
 *               0.00100000000000000
 *           ]
 *        ],
 *       "asks" : [
 *       ]
 *   }
 * ]
 */
extern json_spirit::Value dxGetOrderBook(const json_spirit::Array& params, bool fHelp);


// in rest.cpp
extern bool HTTPReq_REST(AcceptedConnection* conn,
    std::string& strURI,
    std::map<std::string, std::string>& mapHeaders,
    bool fRun);

#endif // BITCOIN_RPCSERVER_H
