//******************************************************************************
//******************************************************************************

#ifndef XBRIDGEWALLETCONNECTORBCH_H
#define XBRIDGEWALLETCONNECTORBCH_H

#include "xbridgewalletconnectorbtc.h"
#include "xbridgecryptoproviderbtc.h"

//*****************************************************************************
//*****************************************************************************
namespace xbridge
{

//******************************************************************************
//******************************************************************************
class BchWalletConnector : public BtcWalletConnector<BtcCryptoProvider>
{
public:
    BchWalletConnector();

public:
    bool createRefundTransaction(const std::vector<XTxIn> & inputs,
                                 const std::vector<std::pair<std::string, double> > & outputs,
                                 const std::vector<unsigned char> & mpubKey,
                                 const std::vector<unsigned char> & mprivKey,
                                 const std::vector<unsigned char> & innerScript,
                                 const uint32_t lockTime,
                                 std::string & txId,
                                 std::string & rawTx);

    bool createPaymentTransaction(const std::vector<XTxIn> & inputs,
                                  const std::vector<std::pair<std::string, double> > & outputs,
                                  const std::vector<unsigned char> & mpubKey,
                                  const std::vector<unsigned char> & mprivKey,
                                  const std::vector<unsigned char> & xpubKey,
                                  const std::vector<unsigned char> & innerScript,
                                  std::string & txId,
                                  std::string & rawTx);
};

} // namespace xbridge

#endif // XBRIDGEWALLETCONNECTORBCH_H
