// Copyright (c) 2018 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CURRENCYPAIR_H
#define CURRENCYPAIR_H

#include <boost/date_time/posix_time/ptime.hpp>
using boost::posix_time::ptime;

#include <cstdint>
#include <string>

using xid_t = std::string;
using currencySymbol = std::string;

/**
 * @brief common structure for currency pair trade details from local history or blockchain
 */
class CurrencyPair {
public:
    // types
    using amount_t = uint64_t;

    // variables
    ptime timeStamp{};
    std::string xid_or_error;
    std::string fromCurrency;
    amount_t fromAmount;
    std::string toCurrency;
    amount_t toAmount;
    enum class Tag { Empty, Error, Valid } tag;

    // ctors
    CurrencyPair() : tag{Tag::Empty} {}
    CurrencyPair(const std::string& e) : xid_or_error{e}, tag{Tag::Error} {}
    CurrencyPair(const xid_t& xid,
                 std::string fromCurrency, amount_t fromAmount,
                 std::string toCurrency, amount_t toAmount, ptime ts = ptime{})
        : timeStamp(ts), xid_or_error(xid)
        , fromCurrency(fromCurrency), fromAmount(fromAmount)
        , toCurrency(toCurrency), toAmount(toAmount)
        , tag{Tag::Valid}
    {}

    // accessors
    std::string xid() const { return tag == Tag::Valid ? xid_or_error : std::string{}; }
    std::string error() const { return tag == Tag::Error ? xid_or_error : std::string{}; }
};

#endif // CURRENCYPAIR_H
