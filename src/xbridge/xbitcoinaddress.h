// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//*****************************************************************************
//*****************************************************************************

#ifndef BLOCKNET_XBRIDGE_XBITCOINADDRESS_H
#define BLOCKNET_XBRIDGE_XBITCOINADDRESS_H

//*****************************************************************************
//*****************************************************************************
#include <pubkey.h>
#include <script/standard.h>
#include <support/allocators/zeroafterfree.h>

namespace xbridge
{

/**
 * Base class for all base58-encoded data
 */
class CBase58Data
{
protected:
    //! the version byte(s)
    std::vector<unsigned char> vchVersion;

    //! the actually encoded data
    typedef std::vector<unsigned char, zero_after_free_allocator<unsigned char> > vector_uchar;
    vector_uchar vchData;

    CBase58Data();
    void SetData(const std::vector<unsigned char>& vchVersionIn, const void* pdata, size_t nSize);
    void SetData(const std::vector<unsigned char>& vchVersionIn, const unsigned char* pbegin, const unsigned char* pend);

public:
    bool SetString(const char* psz, unsigned int nVersionBytes = 1);
    bool SetString(const std::string& str);
    std::string ToString() const;
    int CompareTo(const CBase58Data& b58) const;

    bool operator==(const CBase58Data& b58) const { return CompareTo(b58) == 0; }
    bool operator<=(const CBase58Data& b58) const { return CompareTo(b58) <= 0; }
    bool operator>=(const CBase58Data& b58) const { return CompareTo(b58) >= 0; }
    bool operator<(const CBase58Data& b58) const { return CompareTo(b58) < 0; }
    bool operator>(const CBase58Data& b58) const { return CompareTo(b58) > 0; }
};

//*****************************************************************************
//*****************************************************************************
class XBitcoinAddress : public CBase58Data
{
public:

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
            return {};
        }

        uint160 id;
        memcpy(&id, &vchData[0], 20);
        return CKeyID(id);
    }
};

} // namespace

#endif // BLOCKNET_XBRIDGE_XBITCOINADDRESS_H
