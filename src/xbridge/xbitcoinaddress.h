//*****************************************************************************
//*****************************************************************************

#ifndef XBITCOINADDRESS_H
#define XBITCOINADDRESS_H

//*****************************************************************************
//*****************************************************************************
#include "base58.h"

namespace xbridge
{

//*****************************************************************************
//*****************************************************************************
class XBitcoinAddress : public CBase58Data
{
public:
    enum
    {
        PUBKEY_ADDRESS = 75,  // XCurrency: address begin with 'X'
        SCRIPT_ADDRESS = 8,
        PUBKEY_ADDRESS_TEST = 111,
        SCRIPT_ADDRESS_TEST = 196,
    };

    bool Set(const CKeyID &id, const char prefix) {
        std::vector<unsigned char> pref(1, prefix);
        SetData(pref, id.begin(), id.end());
        return true;
    }

    bool Set(const CScriptID &id, const char prefix) {
        std::vector<unsigned char> pref(1, prefix);
        SetData(pref, id.begin(), id.end());
        return true;
    }

//    bool Set(const CTxDestination &dest)
//    {
//        return boost::apply_visitor(CBitcoinAddressVisitor(this), dest);
//    }

    /**
     * @brief IsValid
     * @return  true, if address size correctly
     */
    bool IsValid() const
    {
        unsigned int nExpectedSize = 20;
        return vchData.size() == nExpectedSize;
    }

    XBitcoinAddress() = default;

    /**
     * @brief XBitcoinAddress  construct new XBircoinAddressfrom string
     * @param strAddress new address value
     */
    XBitcoinAddress(const std::string& strAddress)
    {
        SetString(strAddress);
    }

    /**
     * @brief XBitcoinAddress  contruct new XBircoinAddress from C style string
     * @param pszAddress - new address value
     */
    XBitcoinAddress(const char* pszAddress)
    {
        SetString(pszAddress);
    }

    /**
     * @brief Get
     * @return serialized public key
     */
    CKeyID Get() const
    {
        if (!IsValid())
        {
            return CKeyID();
        }

        uint160 id;
        memcpy(&id, &vchData[0], 20);
        return CKeyID(id);
    }
};

} // namespace

#endif // XBITCOINADDRESS_H
