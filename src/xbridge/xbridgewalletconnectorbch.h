// Copyright (c) 2017-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//******************************************************************************
//******************************************************************************

#ifndef BLOCKNET_XBRIDGE_XBRIDGEWALLETCONNECTORBCH_H
#define BLOCKNET_XBRIDGE_XBRIDGEWALLETCONNECTORBCH_H

#include <xbridge/xbridgewalletconnectorbtc.h>
#include <xbridge/xbridgecryptoproviderbtc.h>
#include <xbridge/cashaddr/cashaddrenc.h>

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
    bool init() override;

public:
    bool hasValidAddressPrefix(const std::string & addr) const override;
    std::string fromXAddr(const std::vector<unsigned char> & xaddr) const override;
    std::vector<unsigned char> toXAddr(const std::string & addr) const override;
    std::string scriptIdToString(const std::vector<unsigned char> & id) const override;

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

protected:
    bool replayProtectionEnabled(int256_t medianBlockTime);
    bool checkReplayProtectionEnabled();

protected:
    CashParams params;
    bool replayProtection{false}; // BCH replay protection enforcement (SCRIPT_ENABLE_REPLAY_PROTECTION)
};

} // namespace xbridge

#endif // BLOCKNET_XBRIDGE_XBRIDGEWALLETCONNECTORBCH_H
