// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//******************************************************************************
//******************************************************************************

#ifndef BLOCKNET_XBRIDGE_BITCOINRPCCONNECTOR_H
#define BLOCKNET_XBRIDGE_BITCOINRPCCONNECTOR_H

#include <xbridge/xbridgewallet.h>

#include <script/script.h>

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
    bool createFeeTransaction(const CScript & dstScript, const double amount,
                              const double feePerByte,
                              const std::vector<unsigned char> & data,
                              std::vector<xbridge::wallet::UtxoEntry> & availUtxos,
                              std::set<xbridge::wallet::UtxoEntry> & feeUtxos,
                              std::string & rawTx);

    /**
     * @brief Get the unspent p2pkh utxos from the wallet.
     * @param utxos p2pkh, this list is mutated
     * @return false if returned utxo list is empty
     */
    bool unspentP2PKH(std::vector<xbridge::wallet::UtxoEntry> & utxos);

} // namespace rpc

} // namespace xbridge

#endif // BLOCKNET_XBRIDGE_BITCOINRPCCONNECTOR_H
