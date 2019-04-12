#ifndef _XROUTER_CONNECTOR_H_
#define _XROUTER_CONNECTOR_H_

#include "xrouterutils.h"
#include "xbridge/xbridgewallet.h"

#include "streams.h"
#include "wallet.h"

#include <vector>
#include <string>
#include <cstdint>

#include <boost/container/map.hpp>

namespace xrouter
{

class WalletConnectorXRouter : public xbridge::WalletParam
{
public:
    WalletConnectorXRouter();

    WalletConnectorXRouter & operator = (const WalletParam & other)
    {
        *(WalletParam *)this = other;
        return *this;
    }

    virtual std::string              getBlockCount() const = 0;
    virtual std::string              getBlockHash(const int & block) const = 0;
    virtual std::string              getBlock(const std::string & blockHash) const = 0;
    virtual std::vector<std::string> getBlocks(const std::vector<std::string> & blockHashes) const = 0;
    virtual std::string              getTransaction(const std::string & hash) const = 0;
    virtual std::vector<std::string> getTransactions(const std::vector<std::string> & txHashes) const = 0;
    virtual std::vector<std::string> getTransactionsBloomFilter(const int & number, CDataStream & stream, const int & fetchlimit=0) const = 0;
    virtual std::string              sendTransaction(const std::string & transaction) const = 0;
    virtual std::string              decodeRawTransaction(const std::string & hex) const = 0;
    virtual std::string              convertTimeToBlockCount(const std::string & timestamp) const = 0;
    virtual std::string              getBalance(const std::string & address) const = 0;
};


} // namespace xrouter


#endif
