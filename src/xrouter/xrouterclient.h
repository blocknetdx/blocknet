// Copyright (c) 2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_XROUTER_XROUTERCLIENT_H
#define BLOCKNET_XROUTER_XROUTERCLIENT_H

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif
#include <init.h>
#include <interfaces/chain.h>
#include <pubkey.h>
#include <scheduler.h>
#include <servicenode/servicenodemgr.h>
#include <univalue.h>
#include <xrouter/xrouterpeermgr.h>
#include <xrouter/xrouterquerymgr.h>
#include <xrouter/xroutersnodeconfig.h>

namespace xrouter {

class XRouterClient {
public:
    explicit XRouterClient(int argc, char* argv[], CConnman::Options connOpts);
    explicit XRouterClient(CConnman::Options connOpts);
    ~XRouterClient();
    bool start(std::string & error);
    bool start();
    bool stop(std::string & error);
    bool stop();
    bool waitForService(unsigned int timeoutMsec = 30000, const std::vector<std::pair<std::string, int>> & services = {}, unsigned int sleepTimeMsec = 100);
    bool waitForService(unsigned int timeoutMsec = 30000, const std::string & service = "", int serviceCount = 1, unsigned int sleepTimeMsec = 100);
    std::vector<sn::ServiceNode> getServiceNodes();

public:
    std::string getBlockCountRaw(const std::string & currency, std::string & uuid, int querynodes = 1);
    UniValue getBlockCount(const std::string & currency, std::string & uuid, int querynodes = 1);
    UniValue getBlockCount(const std::string & currency, int querynodes = 1);

    std::string getBlockHashRaw(const std::string & currency, unsigned int block, std::string & uuid, int querynodes = 1);
    UniValue getBlockHash(const std::string & currency, unsigned int block, std::string & uuid, int querynodes = 1);
    UniValue getBlockHash(const std::string & currency, unsigned int block, int querynodes = 1);
    std::string getBlockHashRaw(const std::string & currency, const std::string & block, std::string & uuid, int querynodes = 1);
    UniValue getBlockHash(const std::string & currency, const std::string & block, std::string & uuid, int querynodes = 1);
    UniValue getBlockHash(const std::string & currency, const std::string & block, int querynodes = 1);

    std::string getBlockRaw(const std::string & currency, unsigned int block, std::string & uuid, int querynodes = 1);
    UniValue getBlock(const std::string & currency, unsigned int block, std::string & uuid, int querynodes = 1);
    UniValue getBlock(const std::string & currency, unsigned int block, int querynodes = 1);
    std::string getBlockRaw(const std::string & currency, const std::string & block, std::string & uuid, int querynodes = 1);
    UniValue getBlock(const std::string & currency, const std::string & block, std::string & uuid, int querynodes = 1);
    UniValue getBlock(const std::string & currency, const std::string & block, int querynodes = 1);

    std::string getBlocksRaw(const std::string & currency, const std::vector<unsigned int> & blocks, std::string & uuid, int querynodes = 1);
    UniValue getBlocks(const std::string & currency, const std::vector<unsigned int> & blocks, std::string & uuid, int querynodes = 1);
    UniValue getBlocks(const std::string & currency, const std::vector<unsigned int> & blocks, int querynodes = 1);
    std::string getBlocksRaw(const std::string & currency, const std::vector<std::string> & blocks, std::string & uuid, int querynodes = 1);
    UniValue getBlocks(const std::string & currency, const std::vector<std::string> & blocks, std::string & uuid, int querynodes = 1);
    UniValue getBlocks(const std::string & currency, const std::vector<std::string> & blocks, int querynodes = 1);

    std::string getTransactionRaw(const std::string & currency, const std::string & transaction, std::string & uuid, int querynodes = 1);
    UniValue getTransaction(const std::string & currency, const std::string & transaction, std::string & uuid, int querynodes = 1);
    UniValue getTransaction(const std::string & currency, const std::string & transaction, int querynodes = 1);

    std::string getTransactionsRaw(const std::string & currency, const std::vector<std::string> & txns, std::string & uuid, int querynodes = 1);
    UniValue getTransactions(const std::string & currency, const std::vector<std::string> & txns, std::string & uuid, int querynodes = 1);
    UniValue getTransactions(const std::string & currency, const std::vector<std::string> & txns, int querynodes = 1);

    std::string decodeTransactionRaw(const std::string & currency, const std::string & rawtransaction, std::string & uuid, int querynodes = 1);
    UniValue decodeTransaction(const std::string & currency, const std::string & rawtransaction, std::string & uuid, int querynodes = 1);
    UniValue decodeTransaction(const std::string & currency, const std::string & rawtransaction, int querynodes = 1);

    std::string sendTransactionRaw(const std::string & currency, const std::string & rawtransaction, std::string & uuid, int querynodes = 1);
    UniValue sendTransaction(const std::string & currency, const std::string & rawtransaction, std::string & uuid, int querynodes = 1);
    UniValue sendTransaction(const std::string & currency, const std::string & rawtransaction, int querynodes = 1);

    std::string callServiceRaw(const std::string & service, std::string & uuid, const UniValue & call_params, int querynodes = 0);
    std::string callServiceRaw(const std::string & service, std::string & uuid, const std::vector<std::string> & call_params, int querynodes = 0);
    UniValue callService(const std::string & service, std::string & uuid, const UniValue & call_params, int querynodes = 0);
    UniValue callService(const std::string & service, std::string & uuid, const std::vector<std::string> & call_params, int querynodes = 0);
    UniValue callService(const std::string & service, const UniValue & call_params, int querynodes = 0);
    UniValue callService(const std::string & service, const std::vector<std::string> & call_params, int querynodes = 0);
    UniValue callService(const std::string & service, const std::string & param1, int querynodes = 0);
    UniValue callService(const std::string & service, const std::string & param1, const std::string & param2, int querynodes = 0);
    UniValue callService(const std::string & service, const std::string & param1, const std::string & param2, const std::string & param3, int querynodes = 0);
    UniValue callService(const std::string & service, const std::string & param1, const std::string & param2, const std::string & param3, const std::string & param4, int querynodes = 0);
    UniValue callService(const std::string & service, int querynodes = 0);

public: // static
    static CConnman::Options defaultOptions();

protected: // static
    static UniValue uret(const std::string & res, const std::string & uuid);

protected:
    int runningCount(const std::string & service = "");
    std::string xrouterCall(enum XRouterCommand command, std::string & uuidRet, const std::string & fqServiceName,
            const int & confirmations, const UniValue & params);

protected:
    sn::ServiceNodeMgr & smgr;
    CConnman::Options connOptions;
    CScheduler scheduler;
    InitInterfaces interfaces;
    boost::thread_group threadGroup;
    std::unique_ptr<XRouterPeerMgr> peerMgr;
    std::unique_ptr<ECCVerifyHandle> globalVerifyHandle;
    QueryMgr queryMgr;
    std::unique_ptr<XRouterSettings> xrsettings;
    std::map<NodeAddr, XRouterSnodeConfigPtr> settings;
    CKey clientKey;
    std::vector<unsigned char> cpubkey;
    std::vector<unsigned char> cprivkey;
    std::atomic<bool> started{false};
};

}

#endif // BLOCKNET_XROUTER_XROUTERCLIENT_H