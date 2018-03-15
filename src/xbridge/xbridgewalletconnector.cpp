//*****************************************************************************
//*****************************************************************************

#include "xbridgewalletconnector.h"
#include "xbridgetransactiondescr.h"
#include "base58.h"

//*****************************************************************************
//*****************************************************************************
namespace xbridge
{

//*****************************************************************************
//*****************************************************************************
namespace wallet
{

//*****************************************************************************
//*****************************************************************************
std::string UtxoEntry::toString() const
{
    std::ostringstream o;
    o << txId << ":" << vout << ":" << amount << ":" << address;
    return o.str();
}

} // namespace wallet

//*****************************************************************************
//*****************************************************************************
WalletConnector::WalletConnector()
{
}

//******************************************************************************
//******************************************************************************
double WalletConnector::getWalletBalance() const
{
    std::vector<wallet::UtxoEntry> entries;
    if (!getUnspent(entries))
    {
        LOG() << "getUnspent failed " << __FUNCTION__;
        return -1.;//return negative value for check in called methods
    }

    double amount = 0;
    for (const wallet::UtxoEntry & entry : entries)
    {
        amount += entry.amount;
    }

    return amount;
}

/**
 * \brief Checks if specified address has a valid prefix.
 * \param addr Address to check
 * \return returns true if address has a valid prefix, otherwise false.
 *
 * If the specified wallet address has a valid prefix the method returns true, otherwise false.
 */
bool WalletConnector::hasValidAddressPrefix(const std::string &addr) const {
    std::vector<unsigned char> decoded;
    if (!DecodeBase58Check(addr, decoded))
    {
        return false;
    }

    bool isP2PKH = memcmp(addrPrefix,   &decoded[0], decoded.size()-sizeof(uint160)) == 0;
    bool isP2SH  = memcmp(scriptPrefix, &decoded[0], decoded.size()-sizeof(uint160)) == 0;

    return isP2PKH || isP2SH;
}

} // namespace xbridge
