// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_XBRIDGE_CURRENCYPAIR_H
#define BLOCKNET_XBRIDGE_CURRENCYPAIR_H

#include <xbridge/currency.h>

#include <string>

#include <boost/date_time/posix_time/ptime.hpp>

using xid_t = std::string;

/**
 * @brief common structure for currency pair trade details from local history or blockchain
 */
class CurrencyPair {
public:

    // variables
    boost::posix_time::ptime timeStamp{};
    std::string xid_or_error{};
    ccy::Asset from{};
    ccy::Asset to{};
    enum class Tag{Empty, Error, Valid} tag{Tag::Empty};

    // ctors
    CurrencyPair() = default;
    CurrencyPair(const std::string& e) : xid_or_error{e}, tag{Tag::Error} {}
    CurrencyPair(const xid_t& xid, const ccy::Asset& from,
                 const ccy::Asset& to, boost::posix_time::ptime ts = boost::posix_time::ptime{})
        : timeStamp(ts), xid_or_error(xid), from(from), to(to), tag{Tag::Valid}
    {}

    // accessors
    template<typename T = double> T price() const {
        return ccy::Asset::Price<T>{to, from}.value();
    }
    std::string xid() const { return tag == Tag::Valid ? xid_or_error : std::string{}; }
    std::string error() const { return tag == Tag::Error ? xid_or_error : std::string{}; }
};

#endif // BLOCKNET_XBRIDGE_CURRENCYPAIR_H
