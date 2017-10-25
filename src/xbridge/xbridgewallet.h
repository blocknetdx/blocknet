//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGEWALLET_H
#define XBRIDGEWALLET_H

#include <string>
#include <vector>
#include <stdint.h>
#include <cstring>

//*****************************************************************************
//*****************************************************************************
class WalletParam
{
public:
    WalletParam()
        : txVersion(1)
        , COIN(0)
        , minTxFee(0)
        , feePerByte(200)
        , m_minAmount(0)
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

    WalletParam & operator = (const WalletParam & other)
    {
        title                       = other.title;
        currency                    = other.currency;
        address                     = other.address;

        m_ip                        = other.m_ip;
        m_port                      = other.m_port;
        m_user                      = other.m_user;
        m_passwd                    = other.m_passwd;

        memcpy(addrPrefix,   other.addrPrefix,   sizeof(addrPrefix)*sizeof(addrPrefix[0]));
        memcpy(scriptPrefix, other.scriptPrefix, sizeof(scriptPrefix)*sizeof(scriptPrefix[0]));
        memcpy(secretPrefix, other.secretPrefix, sizeof(secretPrefix)*sizeof(secretPrefix[0]));

        taxaddr                     = other.taxaddr;
        txVersion                   = other.txVersion;
        COIN                        = other.COIN;
        minTxFee                    = other.minTxFee;
        feePerByte                  = other.feePerByte;
        m_minAmount                 = other.m_minAmount;
        dustAmount                  = other.dustAmount;
        method                      = other.method;
        isGetNewPubKeySupported     = other.isGetNewPubKeySupported;
        isImportWithNoScanSupported = other.isImportWithNoScanSupported;
        blockTime                   = other.blockTime;
        requiredConfirmations       = other.requiredConfirmations;
        // serviceNodeFee = other.serviceNodeFee;

        return *this;
    }

    double      minAmount() const { return (double)m_minAmount / 100000; }

// TODO temporary public
public:
    std::string                title;
    std::string                currency;

    std::string                address;

    std::string                m_ip;
    std::string                m_port;
    std::string                m_user;
    std::string                m_passwd;

    char                       addrPrefix[8];
    char                       scriptPrefix[8];
    char                       secretPrefix[8];
    std::string                taxaddr;
    uint32_t                   txVersion;
    uint64_t                   COIN;
    uint64_t                   minTxFee;
    uint64_t                   feePerByte;
    uint64_t                   m_minAmount;
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
};


#endif // XBRIDGEWALLET_H
