//******************************************************************************
//******************************************************************************

#ifndef _BITCOINRPCCONNECTOR_H_
#define _BITCOINRPCCONNECTOR_H_

#include <vector>
#include <string>
#include <cstdint>

#include "xbridgewallet.h"

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
     * @brief Create the Service Node fee payment transaction.
     * @param dstScript
     * @param amount
     * @param data
     * @param feeUtxos
     * @param rawTx
     * @return
     */
    bool createFeeTransaction(const std::vector<unsigned char> & dstScript, const double amount,
                              const std::vector<unsigned char> & data,
                              std::set<xbridge::wallet::UtxoEntry> & feeUtxos,
                              std::string & rawTx);

    /**
     * @brief storeDataIntoBlockchain Submits the Service Node order fee to the network.
     * @param rawTx
     * @param txid
     * @return
     */
    bool storeDataIntoBlockchain(const std::string & rawTx, std::string & txid);

} // namespace rpc

} // namespace xbridge

#endif // _BITCOINRPCCONNECTOR_H_
