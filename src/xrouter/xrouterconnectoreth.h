//******************************************************************************
//******************************************************************************

#ifndef _XROUTER_CONNECTOR_ETH_H_
#define _XROUTER_CONNECTOR_ETH_H_

#include <vector>
#include <string>
#include <cstdint>
#include <sstream>
#include "json/json_spirit.h"
#include "xrouterconnector.h"
#include "streams.h"

using namespace json_spirit;

namespace xrouter
{

class EthWalletConnectorXRouter : public WalletConnectorXRouter {
public:
    std::string getBlockCount() const;
    Object getBlockHash(const std::string & blockId) const;
    Object      getBlock(const std::string & blockHash) const;
    Object      getTransaction(const std::string & trHash) const;
    Array       getAllBlocks(const int number, int blocklimit=0) const;
    Array       getAllTransactions(const std::string & account, const int number, const int time=0, int blocklimit=0) const;
    std::string getBalance(const std::string & account, const int time=0, int blocklimit=0) const;
    std::string getBalanceUpdate(const std::string & account, const int number, const int time=0, int blocklimit=0) const;
    Array       getTransactionsBloomFilter(const int, CDataStream &, int blocklimit=0) const;
    Object      sendTransaction(const std::string &) const;
    std::string convertTimeToBlockCount(const std::string & timestamp) const;
};


} // namespace xrouter

#endif
