//******************************************************************************
//******************************************************************************

#ifndef _XROUTER_CONNECTOR_ETH_H_
#define _XROUTER_CONNECTOR_ETH_H_

#include "xrouterconnector.h"

#include "streams.h"

#include <vector>
#include <string>
#include <cstdint>
#include <sstream>

namespace xrouter
{

class EthWalletConnectorXRouter : public WalletConnectorXRouter {
public:
    std::string              getBlockCount() const override;
    std::string              getBlockHash(const int & block) const override;
    std::string              getBlock(const std::string & hash) const override;
    std::vector<std::string> getBlocks(const std::vector<std::string> & blockHashes) const override;
    std::string              getTransaction(const std::string & hash) const override;
    std::vector<std::string> getTransactions(const std::vector<std::string> & txHashes) const override;
    std::vector<std::string> getTransactionsBloomFilter(const int &, CDataStream &, const int & fetchlimit=0) const override;
    std::string              sendTransaction(const std::string & rawtx) const override;
    std::string              decodeRawTransaction(const std::string & hex) const override;
    std::string              convertTimeToBlockCount(const std::string & timestamp) const override;
    std::string              getBalance(const std::string & address) const override;
};


} // namespace xrouter

#endif
