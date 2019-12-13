// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//*****************************************************************************
//*****************************************************************************

#include <xbridge/xbridgewalletconnector.h>
#include <xbridge/util/logger.h>

#include <base58.h>

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
 * \param excluded List of utxos to exclude
 * \param addr Optional address to filter balance
 * \return returns the wallet balance for the address.
 *
 * The wallet balance for the specified address will be returned. Only utxo's associated with the address
 * are included.
 */
double WalletConnector::getWalletBalance(const std::set<wallet::UtxoEntry> & excluded, const std::string & addr) const
{
    std::vector<wallet::UtxoEntry> entries;
    if (!getUnspent(entries, excluded))
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

/**
 * \brief Generate and return new wallet address
 */
std::string WalletConnector::getNewTokenAddress()
{
    std::string newaddress = "";
    if (!getNewAddress(newaddress))
    {
        LOG() << "getNewAddress failed " << __FUNCTION__;
        return "";
    }

    return newaddress;
}

} // namespace xbridge
