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
double WalletConnector::getWalletBalance(const std::string &addr) const
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
        if (!addr.empty() && entry.address != addr) // exclude utxo's not matching address
            continue;
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
bool WalletConnector::hasValidAddressPrefix(const std::string &addr) const
{
    std::vector<unsigned char> decoded;
    if (!DecodeBase58Check(addr, decoded))
    {
        return false;
    }

    bool isP2PKH = memcmp(addrPrefix,   &decoded[0], decoded.size()-sizeof(uint160)) == 0;
    bool isP2SH  = memcmp(scriptPrefix, &decoded[0], decoded.size()-sizeof(uint160)) == 0;

    return isP2PKH || isP2SH;
}

template <typename T>
T random_element(T begin, T end)
{
    const unsigned long n = std::distance(begin, end);
    const unsigned long divisor = (RAND_MAX + 1) / n;

    unsigned long k;
    do { k = std::rand() / divisor; } while (k >= n);

    std::advance(begin, k);
    return begin;
}

bool WalletConnector::getUtxoEntriesForAmount(const uint64_t& amount, std::vector<wallet::UtxoEntry>& entries) const
{
    uint64_t fee2 = minTxFee2(1, 1) * TransactionDescr::COIN;
    uint64_t fee1 = 0;

    std::vector<wallet::UtxoEntry> outputs;
    getUnspent(outputs);

    if(outputs.empty())
    {
        LOG() << "outputs list are empty " << __FUNCTION__;
        return false;
    }

    //sort entries from smaller to larger
    std::sort(outputs.begin(), outputs.end(),
        [](const wallet::UtxoEntry & a,  const wallet::UtxoEntry & b) {
            return (a.amount) < (b.amount);
        });

    //one output that larger than target value
    std::vector<wallet::UtxoEntry> greaterThanTargetOutput;

    //try to find best matching one output or one larger output
    fee1 = minTxFee1(1, 3) * TransactionDescr::COIN;
    for(const wallet::UtxoEntry & entry : outputs)
    {
        uint64_t utxoAmount = (entry.amount * TransactionDescr::COIN);
        uint64_t fullAmount = amount + fee1 + fee2;

        if(utxoAmount == fullAmount &&
           !isDustAmount(static_cast<double>(utxoAmount - fullAmount) / TransactionDescr::COIN))
        {
            //we are lucky
            entries.emplace_back(entry);
            return true;
        }

        if (utxoAmount > fullAmount &&
            !isDustAmount(static_cast<double>(utxoAmount - fullAmount) / TransactionDescr::COIN))
        {
            greaterThanTargetOutput.emplace_back(entry);
            break;
        }
    }

    //try to find sum of smaller outputs that match target
    std::vector<wallet::UtxoEntry> outputsSmallerThanTarget;
    std::copy_if(outputs.begin(), outputs.end(), std::inserter(outputsSmallerThanTarget, outputsSmallerThanTarget.end()),
                 [&amount](const wallet::UtxoEntry & entry){
        return amount > entry.amount * TransactionDescr::COIN;
    });

    bool smallerOutputsLargerThanTarget = false;

    {
        uint64_t utxoAmount = 0;

        for(const wallet::UtxoEntry & entry : outputsSmallerThanTarget)
        {
            std::vector<wallet::UtxoEntry> outputsForUse;
            outputsForUse.push_back(entry);

            fee1 = minTxFee1(outputsForUse.size(), 3) * TransactionDescr::COIN;

            utxoAmount = (entry.amount * TransactionDescr::COIN);
        }

        uint64_t fullAmount = amount + fee1 + fee2;

        if (utxoAmount >= fullAmount &&
            !isDustAmount(static_cast<double>(utxoAmount - fullAmount) / TransactionDescr::COIN))
            smallerOutputsLargerThanTarget = true;
    }

    //sum of all smaller outputs is lower than target, so return greater output
    if(!smallerOutputsLargerThanTarget)
    {
        if(greaterThanTargetOutput.empty())
        {
            LOG() << "can't make any list of utxo's " << __FUNCTION__;
            return false;
        }

        entries = greaterThanTargetOutput;
        return true;
    }

    //try to combine small outputs to target sum
    std::vector<wallet::UtxoEntry> bestSmallerOutputsCombination;
    uint64_t smallestFee = std::numeric_limits<uint64_t>::max();
    const uint32_t iterations = 1000;
    for(uint32_t i = 0; i < iterations; ++i)
    {
        std::vector<wallet::UtxoEntry> uniqueOutputsSmallerThanTarget(outputsSmallerThanTarget);
        std::vector<wallet::UtxoEntry> outputsForUse;

        while (!uniqueOutputsSmallerThanTarget.empty())
        {
            const auto it = random_element(uniqueOutputsSmallerThanTarget.begin(),
                                           uniqueOutputsSmallerThanTarget.end());
            wallet::UtxoEntry entry = *it;

            uniqueOutputsSmallerThanTarget.erase(it);

            outputsForUse.push_back(entry);

            fee1 = minTxFee1(outputsForUse.size(), 3) * TransactionDescr::COIN;

            uint64_t utxoAmount = (entry.amount * TransactionDescr::COIN);

            uint64_t fullAmount = amount + fee1 + fee2;

            if (utxoAmount >= fullAmount &&
                !isDustAmount(static_cast<double>(utxoAmount - fullAmount) / TransactionDescr::COIN))
            {
                if(fee1 < smallestFee)
                {
                    smallestFee = fee1;
                    bestSmallerOutputsCombination = outputsForUse;
                    break;
                }
            }
        }
    }


    if(greaterThanTargetOutput.empty() && bestSmallerOutputsCombination.empty())
    {
        LOG() << "all strategy are fail to create utxo's list " << __FUNCTION__;
        return false;
    }
    else if(greaterThanTargetOutput.empty())
        entries = bestSmallerOutputsCombination;
    else if(bestSmallerOutputsCombination.empty())
        entries = greaterThanTargetOutput;
    else
    {
        uint64_t greaterThanTargetOutputValue = 0;
        uint64_t bestSmallerOutputsCombinationValue = 0;

        for(const wallet::UtxoEntry & entry : greaterThanTargetOutput)
            greaterThanTargetOutputValue += (entry.amount * TransactionDescr::COIN);

        for(const wallet::UtxoEntry & entry : bestSmallerOutputsCombination)
            bestSmallerOutputsCombinationValue += (entry.amount * TransactionDescr::COIN);

        if(greaterThanTargetOutputValue < bestSmallerOutputsCombinationValue)
            entries = greaterThanTargetOutput;
        else if(greaterThanTargetOutputValue > bestSmallerOutputsCombinationValue)
            entries = bestSmallerOutputsCombination;
    }

    return true;
}

} // namespace xbridge
