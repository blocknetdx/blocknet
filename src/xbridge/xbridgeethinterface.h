//******************************************************************************
//******************************************************************************

#ifndef XBRIDGEETHINTERFACE_H
#define XBRIDGEETHINTERFACE_H

#include "xbridgeethencoder.h"
#include "uint256.h"

//******************************************************************************
//******************************************************************************
namespace xbridge
{

//******************************************************************************
// temporary interface for eth/erc20
// must be killed after refactoring
//******************************************************************************
class EthWalletInterface
{
public:
    virtual bool getGasPrice(uint256 & gasPrice) const = 0;

    virtual bool getEstimateGas(const bytes & myAddress,
                                const bytes & data,
                                const uint256 & value,
                                uint256 & estimateGas) const = 0;

    virtual bool getBalance(const bytes & account, uint256 & balance) const = 0;

    virtual bool getLastBlockTime(uint256 & blockTime) const = 0;

    virtual bytes createInitiateData(const uint256 & amount,
                                     const bytes & hashedSecret,
                                     const bytes & responderAddress,
                                     const uint256 & refundDuration) const = 0;

    virtual bytes createRespondData(const uint256 & amount,
                                    const bytes & hashedSecret,
                                    const bytes & initiatorAddress,
                                    const uint256 & refundDuration) const = 0;

    virtual bytes createRefundData(const bytes & hashedSecret) const = 0;
    virtual bytes createRedeemData(const bytes & hashedSecret, const bytes & secret) const = 0;

    virtual bool approve(const uint256 & amount) const = 0;

    virtual bool callContractMethod(const bytes & myAddress,
                                    const bytes & data,
                                    const uint256 & value,
                                    const uint256 & gas,
                                    uint256 & transactionHash) const = 0;

    virtual bool isInitiated(const bytes& hashedSecret, bytes& initiatorAddress, const bytes & responderAddress, const uint256 value) const = 0;
    virtual bool isResponded(const bytes& hashedSecret, const bytes & initiatorAddress, bytes & responderAddress, const uint256 value) const = 0;
    virtual bool isRefunded(const bytes& hashedSecret, const bytes & recipientAddress, const uint256 value) const = 0;
    virtual bool isRedeemed(const bytes& hashedSecret, const bytes & recipientAddress, const uint256 value) const = 0;
};

} // namespace xbridge

#endif