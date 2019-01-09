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
     * @param feePerByte
     * @param data
     * @param availUtxos Available utxos to use with the fee transaction
     * @param feeUtxos Returns the utxos selected for the fee
     * @param rawTx
     * @return
     */
    bool createFeeTransaction(const std::vector<unsigned char> & dstScript, const double amount,
                              const double feePerByte,
                              const std::vector<unsigned char> & data,
                              std::vector<xbridge::wallet::UtxoEntry> & availUtxos,
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
