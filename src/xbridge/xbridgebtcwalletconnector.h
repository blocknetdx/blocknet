//*****************************************************************************
//*****************************************************************************

#ifndef XBRIDGEBTCWALLETCONNECTOR_H
#define XBRIDGEBTCWALLETCONNECTOR_H

#include "xbridgewalletconnector.h"

//*****************************************************************************
//*****************************************************************************
class XBridgeBtcWalletConnector : public XBridgeWalletConnector
{
public:
    XBridgeBtcWalletConnector();

public:
    // reimplement for currency
    // virtual std::string fromXAddr(const std::vector<unsigned char> & xaddr) const = 0;
    virtual std::vector<unsigned char> toXAddr(const std::string & addr) const;

public:
    bool requestAddressBook(std::vector<wallet::AddressBookEntry> & entries);

    bool getUnspent(std::vector<wallet::UtxoEntry> & inputs) const;
    bool lockUnspent(std::vector<wallet::UtxoEntry> & inputs, const bool lock = true) const;
};

#endif // XBRIDGEBTCWALLETCONNECTOR_H
