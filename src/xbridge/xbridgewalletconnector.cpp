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

/**
 * \brief Return the wallet balance; optionally for the specified address.
 * \param addr Optional address to filter balance
 * \return returns the wallet balance for the address.
 *
 * The wallet balance for the specified address will be returned. Only utxo's associated with the address
 * are included.
 */
double WalletConnector::getWalletBalance(const std::string & addr) const
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
        // exclude utxo's not matching address
        if (!addr.empty() && entry.address != addr)
        {
            continue;
        }
        amount += entry.amount;
    }

    return amount;
}

//******************************************************************************
//******************************************************************************
bool WalletConnector::lockCoins(const std::vector<wallet::UtxoEntry> & inputs,
                                const bool lock)
{
    LOCK(lockedCoinsLocker);

    if (!lock)
    {
        for (const wallet::UtxoEntry & entry : inputs)
        {
            lockedCoins.erase(entry);
        }
    }
    else
    {
        // check duplicates
        for (const wallet::UtxoEntry & entry : inputs)
        {
            if (lockedCoins.count(entry))
            {
                return false;
            }
        }

        lockedCoins.insert(inputs.begin(), inputs.end());
    }

    return true;
}

//******************************************************************************
//******************************************************************************
void WalletConnector::removeLocked(std::vector<wallet::UtxoEntry> & inputs) const
{
    LOCK(lockedCoinsLocker);

    for (auto it = inputs.begin(); it != inputs.end(); )
    {
        if (lockedCoins.count(*it))
        {
            it = inputs.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

} // namespace xbridge
