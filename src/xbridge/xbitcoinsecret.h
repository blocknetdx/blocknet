#ifndef XBITCOINSECRET_H
#define XBITCOINSECRET_H

#include "base58.h"
#include "xkey.h"

namespace xbridge
{

//******************************************************************************
//******************************************************************************
class CBitcoinSecret : public CBase58Data
{
public:
    /**
     * @brief SetKey  - set new secret
     * @param vchSecret - vector of char with new data
     */
    void SetKey(const CKey& vchSecret);
    /**
     * @brief GetKey Initialize new CKey using begin and end iterators to byte data.
     * @return  generated CKey
     */
    CKey GetKey();
    /**
     * @brief IsValid - checks expected format and version
     * @return true, if data is valid
     */
    bool IsValid() const;
    /**
     * @brief SetString
     * @param pszSecret
     * @return if the data is recorded and correct
     */
    bool SetString(const char* pszSecret);
    /**
     * @brief SetString set new key value from string
     * @param strSecret - string with secret
     * @return true, if the data is recorded and correct
     */
    bool SetString(const std::string& strSecret);

    /**
     * @brief CBitcoinSecret - construct new bitcoin secret
     * @param vchSecret - new data
     */
    CBitcoinSecret(const CKey& vchSecret) { SetKey(vchSecret); }
    /**
     * @brief CBitcoinSecret - default constructor
     */
    CBitcoinSecret() = default;
};

} // namespace xbridge

#endif // XBITCOINSECRET_H
