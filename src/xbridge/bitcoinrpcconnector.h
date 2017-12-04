//******************************************************************************
//******************************************************************************

#ifndef _BITCOINRPCCONNECTOR_H_
#define _BITCOINRPCCONNECTOR_H_

#include <vector>
#include <string>
#include <cstdint>

//*****************************************************************************
//*****************************************************************************
namespace xbridge
{

//******************************************************************************
//******************************************************************************
namespace rpc
{
//    bool addMultisigAddress(const std::string & rpcuser,
//                            const std::string & rpcpasswd,
//                            const std::string & rpcip,
//                            const std::string & rpcport,
//                            const std::vector<std::string> & keys,
//                            std::string & addr);

//    bool getTransaction(const std::string & rpcuser,
//                        const std::string & rpcpasswd,
//                        const std::string & rpcip,
//                        const std::string & rpcport,
//                        const std::string & txid,
//                        std::string & tx);

//    bool getNewPubKey(const std::string & rpcuser,
//                      const std::string & rpcpasswd,
//                      const std::string & rpcip,
//                      const std::string & rpcport,
//                      std::string & key);

//    bool dumpPrivKey(const std::string & rpcuser,
//                     const std::string & rpcpasswd,
//                     const std::string & rpcip,
//                     const std::string & rpcport,
//                     const std::string & address,
//                     std::string & key);

//    bool importPrivKey(const std::string & rpcuser,
//                       const std::string & rpcpasswd,
//                       const std::string & rpcip,
//                       const std::string & rpcport,
//                       const std::string & key,
//                       const std::string & label,
//                       const bool & noScanWallet = false);

    // ethereum rpc
//    bool eth_gasPrice(const std::string & rpcip,
//                      const std::string & rpcport,
//                      uint64_t & gasPrice);
//    bool eth_accounts(const std::string & rpcip,
//                      const std::string & rpcport,
//                      std::vector<std::string> & addresses);
//    bool eth_getBalance(const std::string & rpcip,
//                        const std::string & rpcport,
//                        const std::string & account,
//                        uint64_t & amount);
//    bool eth_sendTransaction(const std::string & rpcip,
//                             const std::string & rpcport,
//                             const std::string & from,
//                             const std::string & to,
//                             const uint64_t & amount,
//                             const uint64_t & fee);

    // helper fn-s
    bool getNewAddress(std::vector<unsigned char> & addr);
    bool storeDataIntoBlockchain(const std::vector<unsigned char> & dstAddress,
                                 const double amount,
                                 const std::vector<unsigned char> & data,
                                 std::string & txid);
    bool getDataFromTx(const std::string & txid, std::vector<unsigned char> & data);

} // namespace rpc

} // namespace xbridge

#endif // _BITCOINRPCCONNECTOR_H_
