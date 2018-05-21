//******************************************************************************
//******************************************************************************

#ifndef _XROUTER_CONNECTOR_ETH_H_
#define _XROUTER_CONNECTOR_ETH_H_

#include <vector>
#include <string>
#include <cstdint>
#include "../json/json_spirit_reader_template.h"
#include "../json/json_spirit_writer_template.h"
#include "../json/json_spirit_utils.h"
#include "xrouterconnector.h"

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

class EthWalletConnectorXRouter : public WalletConnectorXRouter {
public:
    Object      getBlockCount() const;
    Object      getBlockHash(const std::string & blockId) const;
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
