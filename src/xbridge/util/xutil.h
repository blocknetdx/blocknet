//*****************************************************************************
//*****************************************************************************

#ifndef UTIL_H
#define UTIL_H

#include "uint256.h"
#include "logger.h"
#include "xbridge/xbridgedef.h"
#include <string>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

//*****************************************************************************
//*****************************************************************************
namespace util
{
namespace bpt = boost::posix_time;
    void init();

    std::wstring wide_string(std::string const & s);//, std::locale const &loc);
    // std::string narrow_string(std::wstring const &s, char default_char = '?');//, std::locale const &loc, char default_char = '?');

    std::string mb_string(std::string const & s);
    std::string mb_string(std::wstring const & s);

    std::string base64_encode(const std::vector<unsigned char> & s);
    std::string base64_encode(const std::string & s);
    std::string base64_decode(const std::string & s);

    std::string to_str(const std::vector<unsigned char> & obj);

    template<class _T> std::string to_str(const _T & obj)
    {
        return util::base64_encode(std::string((const char *)(obj.begin()),
                                               (const char *)(obj.end())));
    }


    /**
     * @brief iso8601 - converted boost posix time to string in ISO 8061 format
     * @param time - boost posix time
     * @return string in ISO 8061 format
     */
    const std::string iso8601(const bpt::ptime &time);

    /**
     * @brief tranactionPrice - calculated transaction price in terms of bid price - toAmount/fromAmount
     * @param ptr - pointer to transaction description
     * @return price of transaction
     */
    double price(const xbridge::TransactionDescrPtr ptr);

    /**
     * @brief priceAsk - the inverted price calculation. Used by asks to calculate price in terms of bid price.
     * askFromAmount/askToAmount.
     * @param ptr - pointer to transaction description
     * @return price of transaction
     */
    double priceBid(const xbridge::TransactionDescrPtr ptr);

    boost::uint64_t timeToInt(const bpt::ptime &time);
    bpt::ptime intToTime(const uint64_t& number);

    double xBridgeValueFromAmount(uint64_t amount);

    uint64_t xBridgeAmountFromReal(double val);
} // namespace

#endif // UTIL_H
