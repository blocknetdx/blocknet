// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//******************************************************************************
//******************************************************************************

#ifndef BLOCKNET_XBRIDGE_XBRIDGERPC_H
#define BLOCKNET_XBRIDGE_XBRIDGERPC_H

#include <vector>
#include <string>
#include <cstdint>

//******************************************************************************
//******************************************************************************
namespace rpc
{
    typedef std::pair<std::string, std::vector<std::string> > AddressBookEntry;

    struct Info
    {
        uint32_t blocks;
    };

    struct Unspent
    {
        std::string txId;
        int vout;
        double amount;
    };


    // ethereum rpc
    bool eth_gasPrice(const std::string & rpcip,
                      const std::string & rpcport,
                      uint64_t & gasPrice);
    bool eth_accounts(const std::string & rpcip,
                      const std::string & rpcport,
                      std::vector<std::string> & addresses);
    bool eth_getBalance(const std::string & rpcip,
                        const std::string & rpcport,
                        const std::string & account,
                        uint64_t & amount);
    bool eth_sendTransaction(const std::string & rpcip,
                             const std::string & rpcport,
                             const std::string & from,
                             const std::string & to,
                             const uint64_t & amount,
                             const uint64_t & fee);


} // namespace rpc

#endif
