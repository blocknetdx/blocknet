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
    bool storeDataIntoBlockchain(const std::vector<unsigned char> & dstAddress,
                                 const double amount,
                                 const std::vector<unsigned char> & data,
                                 std::string & txid);
    bool getDataFromTx(const std::string & txid, std::vector<unsigned char> & data);

} // namespace rpc

} // namespace xbridge

#endif // _BITCOINRPCCONNECTOR_H_
