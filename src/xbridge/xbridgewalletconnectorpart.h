// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//******************************************************************************
//******************************************************************************

#ifndef BLOCKNET_XBRIDGE_XBRIDGEWALLETCONNECTORPART_H
#define BLOCKNET_XBRIDGE_XBRIDGEWALLETCONNECTORPART_H

#include <xbridge/xbridgecryptoproviderbtc.h>
#include <xbridge/xbridgewalletconnectorbtc.h>

//*****************************************************************************
//*****************************************************************************
namespace xbridge
{

//******************************************************************************
//******************************************************************************

class PartWalletConnector : public BtcWalletConnector<BtcCryptoProvider>
{
public:
    PartWalletConnector();

public:
    bool createRefundTransaction(const std::vector<XTxIn> & inputs,
                                 const std::vector<std::pair<std::string, double> > & outputs,
                                 const std::vector<unsigned char> & mpubKey,
                                 const std::vector<unsigned char> & mprivKey,
                                 const std::vector<unsigned char> & innerScript,
                                 const uint32_t lockTime,
                                 std::string & txId,
                                 std::string & rawTx) override;

    bool createPaymentTransaction(const std::vector<XTxIn> & inputs,
                                  const std::vector<std::pair<std::string, double> > & outputs,
                                  const std::vector<unsigned char> & mpubKey,
                                  const std::vector<unsigned char> & mprivKey,
                                  const std::vector<unsigned char> & xpubKey,
                                  const std::vector<unsigned char> & innerScript,
                                  std::string & txId,
                                  std::string & rawTx) override;

    bool createPartialTransaction(const std::vector<XTxIn> inputs,
                                  const std::vector<std::pair<std::string, double> > outputs,
                                  std::string & txId,
                                  std::string & rawTx) override;
};

} // namespace xbridge

#endif // BLOCKNET_XBRIDGE_XBRIDGEWALLETCONNECTORPART_H
