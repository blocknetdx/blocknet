//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGEWALLET_H
#define XBRIDGEWALLET_H

#include <string>
#include <vector>
#include <set>
#include <stdint.h>
#include <cstring>

//*****************************************************************************
//*****************************************************************************
namespace xbridge
{

//*****************************************************************************
//*****************************************************************************
namespace wallet
{
typedef std::pair<std::string, std::vector<std::string> > AddressBookEntry;

struct UtxoEntry
{
    std::string txId;
    uint32_t    vout;
    double      amount;
    std::string address;

    std::vector<unsigned char> rawAddress;
    std::vector<unsigned char> signature;

    std::string toString() const;

    bool operator < (const UtxoEntry & r) const
    {
        return (txId < r.txId) || ((txId == r.txId) && (vout < r.vout));
    }
};

} // namespace wallet

//*****************************************************************************
//*****************************************************************************
class WalletParam
{
public:
    WalletParam()
        : txVersion(1)
        , COIN(0)
        , minTxFee(10000)
        , feePerByte(200)
        , dustAmount(0)
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

        txVersion                   = other.txVersion;
        COIN                        = other.COIN;
        minTxFee                    = other.minTxFee;
        feePerByte                  = other.feePerByte;
        dustAmount                  = other.dustAmount;
        method                      = other.method;
        blockTime                   = other.blockTime;
        requiredConfirmations       = other.requiredConfirmations;
        // serviceNodeFee = other.serviceNodeFee;

        return *this;
    }

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
    uint32_t                   txVersion;
    uint64_t                   COIN;
    uint64_t                   minTxFee;
    uint64_t                   feePerByte;
    uint64_t                   dustAmount;
    std::string                method;

    // block time in seconds
    uint32_t                   blockTime;

    // required confirmations for tx
    uint32_t                   requiredConfirmations;

    //service node fee, see rpc::storeDataIntoBlockchain
    const double               serviceNodeFee;
};

} // namespace xbridge

#endif // XBRIDGEWALLET_H
