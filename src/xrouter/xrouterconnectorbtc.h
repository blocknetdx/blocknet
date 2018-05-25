//******************************************************************************
//******************************************************************************

#ifndef _XROUTER_CONNECTOR_BTC_H_
#define _XROUTER_CONNECTOR_BTC_H_

#include <vector>
#include <string>
#include <cstdint>
#include "../json/json_spirit.h"
#include "xrouterconnector.h"

using namespace json_spirit;

//******************************************************************************
//******************************************************************************
namespace rpc
{
    Object CallRPCAuthorized(const std::string & auth,
                             const std::string & rpcip,
                             const std::string & rpcport,
                             const std::string & strMethod,
                             const Array & params);

    Object CallRPC(const std::string & rpcuser,
                   const std::string & rpcpasswd,
                   const std::string & rpcip,
                   const std::string & rpcport,
                   const std::string & strMethod,
                   const Array & params);
} // namespace rpc

namespace xrouter
{

class BtcWalletConnectorXRouter : public WalletConnectorXRouter {
public:
    std::string getBlockCount() const;
    std::string getBlockHash(const std::string & blockId) const;
    Object      getBlock(const std::string & blockHash) const;
    Object      getTransaction(const std::string & trHash) const;
    Array       getAllBlocks(const int number) const;
    Array       getAllTransactions(const std::string & account, const int number) const;
    std::string getBalance(const std::string & account) const;
    std::string getBalanceUpdate(const std::string & account, const int number) const;
    Array       getTransactionsBloomFilter(const int number) const;
    Object      sendTransaction(const std::string & transaction) const;
    Object      getPaymentAddress() const;
};


} // namespace xrouter

#endif
