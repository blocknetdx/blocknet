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
    // helper fn-s
    /**
     * @brief storeDataIntoBlockchain
     * @param dstAddress
     * @param amount
     * @param data
     * @param txid
     * @return
     */
    bool storeDataIntoBlockchain(const std::vector<unsigned char> & dstAddress,
                                 const double amount,
                                 const std::vector<unsigned char> & data,
                                 std::string & txid);
    /**
     * @brief getDataFromTx
     * @param txid
     * @param data
     * @return
     */
    bool getDataFromTx(const std::string & txid, std::vector<unsigned char> & data);

} // namespace rpc

} // namespace xbridge

#endif // _BITCOINRPCCONNECTOR_H_
