//******************************************************************************
//******************************************************************************

#ifndef XBRIDGEWALLETCONNECTORBCC_H
#define XBRIDGEWALLETCONNECTORBCC_H

#include "xbridgewalletconnectorbtc.h"

//******************************************************************************
//******************************************************************************
class XBridgeBccWalletConnector : public XBridgeBtcWalletConnector
{
public:
    XBridgeBccWalletConnector();

public:
    bool createRefundTransaction(const std::vector<std::pair<std::string, int> > & inputs,
                                 const std::vector<std::pair<std::string, double> > & outputs,
                                 const std::vector<unsigned char> & mpubKey,
                                 const std::vector<unsigned char> & mprivKey,
                                 const std::vector<unsigned char> & innerScript,
                                 const uint32_t lockTime,
                                 std::string & txId,
                                 std::string & rawTx);

    bool createPaymentTransaction(const std::vector<std::pair<std::string, int> > & inputs,
                                  const std::vector<std::pair<std::string, double> > & outputs,
                                  const std::vector<unsigned char> & mpubKey,
                                  const std::vector<unsigned char> & mprivKey,
                                  const std::vector<unsigned char> & xpubKey,
                                  const std::vector<unsigned char> & innerScript,
                                  std::string & txId,
                                  std::string & rawTx);
};

#endif // XBRIDGEWALLETCONNECTORBCC_H
