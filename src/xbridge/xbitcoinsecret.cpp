//******************************************************************************
//******************************************************************************

#include "xbitcoinsecret.h"
#include "util/logger.h"
//******************************************************************************
//******************************************************************************
namespace xbridge
{

//******************************************************************************
//******************************************************************************
void CBitcoinSecret::SetKey(const CKey& vchSecret)
{

    if(!vchSecret.IsValid()) {
        DEBUG_TRACE_LOG("invalid key");
        return;
    }
    std::vector<unsigned char> pref(1, 0);
    SetData(pref, vchSecret.begin(), vchSecret.end());
    if (vchSecret.IsCompressed())
        vchData.push_back(1);
}

//******************************************************************************
//******************************************************************************
CKey CBitcoinSecret::GetKey()
{
    CKey ret;
    if(vchData.size() < 32) {
        DEBUG_TRACE_LOG("invalid size");
        return ret;
    }
    ret.Set(vchData.begin(), vchData.begin() + 32, vchData.size() > 32 && vchData[32] == 1);
    return ret;
}

//******************************************************************************
//******************************************************************************
bool CBitcoinSecret::IsValid() const
{
    bool fExpectedFormat = vchData.size() == 32 || (vchData.size() == 33 && vchData[32] == 1);
    bool fCorrectVersion = true; // vchVersion == Params().Base58Prefix(CChainParams::SECRET_KEY);
    return fExpectedFormat && fCorrectVersion;
}

//******************************************************************************
//******************************************************************************
bool CBitcoinSecret::SetString(const char* pszSecret)
{
    return CBase58Data::SetString(pszSecret) && IsValid();
}

//******************************************************************************
//******************************************************************************
bool CBitcoinSecret::SetString(const std::string& strSecret)
{
    return SetString(strSecret.c_str());
}

} // namespace xbridge
