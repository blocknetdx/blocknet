#ifndef XBRIDGEWALLETCONNECTORDGB_H
#define XBRIDGEWALLETCONNECTORDGB_H

#include "xbridgewalletconnectorbtc.h"

//*****************************************************************************
//*****************************************************************************
namespace xbridge
{

//******************************************************************************
//******************************************************************************
class DgbWalletConnector : public BtcWalletConnector
{
public:
    DgbWalletConnector();

public:
    bool createDepositTransaction(const std::vector<std::pair<std::string, int> > & inputs,
                                  const std::vector<std::pair<std::string, double> > & outputs,
                                  std::string & txId,
                                  std::string & rawTx);
};

} // namespace xbridge

#endif // XBRIDGEWALLETCONNECTORDGB_H
