#ifndef XBITCOINSECRET_H
#define XBITCOINSECRET_H

#include "base58.h"
#include "xkey.h"

namespace xbridge
{

//******************************************************************************
//******************************************************************************
/**
 * @brief The CBitcoinSecret class
 */
class CBitcoinSecret : public CBase58Data
{
public:
    /**
     * @brief SetKey - set new secret
     * @param vchSecret - vector of char with new data
     */
    void SetKey(const CKey& vchSecret);
    /**
     * @brief GetKey Initialize new CKey using begin and end iterators to byte data.
     * @return  generated CKey
     */
    CKey GetKey();
    /**
     * @brief IsValid - checks valid of data
     * @return true, data is valid
     */
    bool IsValid() const;

    bool SetString(const char* pszSecret);
    bool SetString(const std::string& strSecret);

    CBitcoinSecret(const CKey& vchSecret) { SetKey(vchSecret); }
    CBitcoinSecret() {}
};

} // namespace xbridge

#endif // XBITCOINSECRET_H
