//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGEWALLET_H
#define XBRIDGEWALLET_H

#include <string>
#include <vector>
#include <stdint.h>

//*****************************************************************************
//*****************************************************************************
struct WalletParam
{
    std::string                title;
    std::string                currency;
    std::string                address;
    std::string                ip;
    std::string                port;
    std::string                user;
    std::string                passwd;
    char                       addrPrefix[8];
    char                       scriptPrefix[8];
    char                       secretPrefix[8];
    std::string                taxaddr;
    uint32_t                   txVersion;
    uint64_t                   COIN;
    uint64_t                   minTxFee;
    uint64_t                   feePerByte;
    uint64_t                   minAmount;
    uint64_t                   dustAmount;
    std::string                method;
    bool                       isGetNewPubKeySupported;
    bool                       isImportWithNoScanSupported;

    // block time in seconds
    uint32_t                   blockTime;

    // required confirmations for tx
    uint32_t                   requiredConfirmations;

    //service node fee, see rpc::storeDataIntoBlockchain
    const double               serviceNodeFee;

    WalletParam()
        : txVersion(1)
        , COIN(0)
        , minTxFee(0)
        , feePerByte(200)
        , minAmount(0)
        , dustAmount(0)
        , isGetNewPubKeySupported(false)
        , isImportWithNoScanSupported(false)
        , blockTime(0)
        , requiredConfirmations(0)
        , serviceNodeFee(.005)
    {
        memset(addrPrefix,   0, sizeof(addrPrefix));
        memset(scriptPrefix, 0, sizeof(scriptPrefix));
        memset(secretPrefix, 0, sizeof(secretPrefix));
    }
};


#endif // XBRIDGEWALLET_H
