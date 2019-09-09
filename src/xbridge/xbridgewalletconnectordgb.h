// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_XBRIDGE_XBRIDGEWALLETCONNECTORDGB_H
#define BLOCKNET_XBRIDGE_XBRIDGEWALLETCONNECTORDGB_H

#include <xbridge/xbridgecryptoproviderbtc.h>
#include <xbridge/xbridgewalletconnectorbtc.h>

//*****************************************************************************
//*****************************************************************************
namespace xbridge
{

//******************************************************************************
//******************************************************************************
class DgbWalletConnector : public BtcWalletConnector<BtcCryptoProvider>
{
public:
    DgbWalletConnector();

public:
    bool createDepositTransaction(const std::vector<XTxIn> & inputs,
                                  const std::vector<std::pair<std::string, double> > & outputs,
                                  std::string & txId,
                                  uint32_t & txVout,
                                  std::string & rawTx);
};

} // namespace xbridge

#endif // BLOCKNET_XBRIDGE_XBRIDGEWALLETCONNECTORDGB_H
