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
    void threadRPCServer();

    class AcceptedConnection;
    void handleRpcRequest(AcceptedConnection * conn);

    std::vector<unsigned char> toXAddr(const std::string & addr);

    typedef std::pair<std::string, std::vector<std::string> > AddressBookEntry;
    bool requestAddressBook(const std::string & rpcuser,
                            const std::string & rpcpasswd,
                            const std::string & rpcip,
                            const std::string & rpcport,
                            std::vector<AddressBookEntry> & entries);

    struct Info
    {
        uint32_t blocks;
    };
    bool getInfo(const std::string & rpcuser,
                 const std::string & rpcpasswd,
                 const std::string & rpcip,
                 const std::string & rpcport,
                 Info & info);

    struct Unspent
    {
        std::string txId;
        int vout;
        double amount;
    };

    bool createRawTransaction(const std::string & rpcuser,
                              const std::string & rpcpasswd,
                              const std::string & rpcip,
                              const std::string & rpcport,
                              const std::vector<std::pair<std::string, int> > & inputs,
                              const std::vector<std::pair<std::string, double> > & outputs,
                              const uint32_t lockTime,
                              std::string & tx,
                              bool cltv);

    bool decodeRawTransaction(const std::string & rpcuser,
                              const std::string & rpcpasswd,
                              const std::string & rpcip,
                              const std::string & rpcport,
                              const std::string & rawtx,
                              std::string & txid,
                              std::string & tx);

    std::string prevtxsJson(const std::vector<std::tuple<std::string, int, std::string, std::string> > & prevtxs);

    bool signRawTransaction(const std::string & rpcuser,
                            const std::string & rpcpasswd,
                            const std::string & rpcip,
                            const std::string & rpcport,
                            std::string & rawtx,
                            bool & complete);

    bool signRawTransaction(const std::string & rpcuser,
                            const std::string & rpcpasswd,
                            const std::string & rpcip,
                            const std::string & rpcport,
                            std::string & rawtx,
                            const std::string & prevtxs,
                            const std::vector<std::string> & keys,
                            bool & complete);

    bool sendRawTransaction(const std::string & rpcuser,
                            const std::string & rpcpasswd,
                            const std::string & rpcip,
                            const std::string & rpcport,
                            const std::string & rawtx);

    bool getNewAddress(const std::string & rpcuser,
                       const std::string & rpcpasswd,
                       const std::string & rpcip,
                       const std::string & rpcport,
                       std::string & addr);

    bool addMultisigAddress(const std::string & rpcuser,
                            const std::string & rpcpasswd,
                            const std::string & rpcip,
                            const std::string & rpcport,
                            const std::vector<std::string> & keys,
                            std::string & addr);

    bool getTransaction(const std::string & rpcuser,
                        const std::string & rpcpasswd,
                        const std::string & rpcip,
                        const std::string & rpcport,
                        const std::string & txid);
                        // std::string & tx);

    bool getNewPubKey(const std::string & rpcuser,
                      const std::string & rpcpasswd,
                      const std::string & rpcip,
                      const std::string & rpcport,
                      std::string & key);

    bool dumpPrivKey(const std::string & rpcuser,
                     const std::string & rpcpasswd,
                     const std::string & rpcip,
                     const std::string & rpcport,
                     const std::string & address,
                     std::string & key);

    bool importPrivKey(const std::string & rpcuser,
                       const std::string & rpcpasswd,
                       const std::string & rpcip,
                       const std::string & rpcport,
                       const std::string & key,
                       const std::string & label,
                       const bool & noScanWallet = false);

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
