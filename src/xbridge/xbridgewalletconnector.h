//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGEWALLETCONNECTOR_H
#define XBRIDGEWALLETCONNECTOR_H

#include "xbridgewallet.h"

#include <vector>
#include <string>
#include <memory>

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

    friend bool operator < (const UtxoEntry & l, const UtxoEntry & r);
    friend bool operator == (const UtxoEntry & l, const UtxoEntry & r);
};

} // namespace wallet

//*****************************************************************************
//*****************************************************************************
class XBridgeWalletConnector : public WalletParam
{
public:
    XBridgeWalletConnector();

public:
    XBridgeWalletConnector & operator = (const WalletParam & other)
    {
        *(WalletParam *)this = other;
        return *this;
    }

public:
    // reimplement for currency
    // virtual std::string fromXAddr(const std::vector<unsigned char> & xaddr) const = 0;
    virtual std::vector<unsigned char> toXAddr(const std::string & addr) const = 0;

public:
    virtual bool requestAddressBook(std::vector<wallet::AddressBookEntry> & entries) = 0;

    virtual bool getUnspent(std::vector<wallet::UtxoEntry> & inputs) const = 0;

    bool checkAmount(const uint64_t amount) const;
    double getWalletBalance() const;

    virtual bool lockUnspent(std::vector<wallet::UtxoEntry> & inputs, const bool lock = true) const = 0;

public:
    virtual bool makeNewKey(std::vector<unsigned char> & /*key*/) { return false; }
};

typedef std::shared_ptr<XBridgeWalletConnector> XBridgeWalletConnectorPtr;

#endif // XBRIDGEWALLETCONNECTOR_H
