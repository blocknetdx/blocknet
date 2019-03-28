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
    std::string getBlockCount() const override;
    Object      getBlockHash(const int & block) const override;
    Object      getBlock(const std::string & hash) const override;
    Array       getBlocks(const std::set<std::string> & blockHashes) const override;
    Object      getTransaction(const std::string & hash) const override;
    Array       getTransactions(const std::set<std::string> & txHashes) const override;
    Array       getTransactionsBloomFilter(const int &, CDataStream &, const int & blocklimit=0) const override;
    Object      sendTransaction(const std::string &) const override;
    std::string convertTimeToBlockCount(const std::string & timestamp) const override;
    std::string getBalance(const std::string & address) const override;
};


} // namespace xrouter

#endif
