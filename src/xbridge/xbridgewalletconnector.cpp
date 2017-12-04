//*****************************************************************************
//*****************************************************************************

#include "xbridgewalletconnector.h"
#include "xbridgetransactiondescr.h"

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
bool operator < (const UtxoEntry & l, const UtxoEntry & r)
{
    return (l.txId < r.txId) || (l.vout < r.vout);
}

//*****************************************************************************
//*****************************************************************************
bool operator == (const UtxoEntry & l, const UtxoEntry & r)
{
    return (l.txId == r.txId) && (l.vout == r.vout);
}

} // namespace wallet

//*****************************************************************************
//*****************************************************************************
WalletConnector::WalletConnector()
{

}

//******************************************************************************
//******************************************************************************
bool WalletConnector::checkAmount(const uint64_t _amount) const
{
    std::vector<wallet::UtxoEntry> inputs;
    // return checkAmountAndGetInputs(amount, inputs);
    inputs.clear();

    double amount = _amount / TransactionDescr::COIN;

    std::vector<wallet::UtxoEntry> entries;
    if (!getUnspent(entries))
    {
        LOG() << "getUnspent failed " << __FUNCTION__;
        return false;
    }

    double funds = 0;
    for (const wallet::UtxoEntry & entry : entries)
    {
        inputs.push_back(entry);

        funds += entry.amount;
        if (amount < funds)
        {
            return true;
        }
    }

    inputs.clear();
    return false;
}

//******************************************************************************
//******************************************************************************
double WalletConnector::getWalletBalance() const
{
    std::vector<wallet::UtxoEntry> entries;
    if (!getUnspent(entries))
    {
        LOG() << "getUnspent failed " << __FUNCTION__;
        return 0;
    }

    double amount = 0;
    for (const wallet::UtxoEntry & entry : entries)
    {
        amount += entry.amount;
    }

    return amount;
}

} // namespace xbridge
