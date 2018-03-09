//*****************************************************************************
//*****************************************************************************

#ifndef UTIL_H
#define UTIL_H

#include "uint256.h"
#include "logger.h"
#include "xbridge/xbridgedef.h"
#include "xbridge/util/xbridgeerror.h"
#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "json/json_spirit_utils.h"

#include <string>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

//*****************************************************************************
//*****************************************************************************
namespace util
{
namespace bpt = boost::posix_time;
using namespace json_spirit;
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
    std::string xBridgeStringValueFromPrice(double price);
    std::string xBridgeStringValueFromAmount(uint64_t amount);

    /**
     * @brief Returns true if the input precision is supported by xbridge.
     * @param coin Coin amount as string
     * @return true if valid, false if invalid
     * Example:<br>
     * \verbatim￼
        xBridgeValidCoin("0.000001")
        // returns true
     * \endverbatim
     */
    bool xBridgeValidCoin(std::string coin);

    /**
     * @brief Returns the number of digits in base 10 integer not including the most significant.
     * @param amount Coin amount as 64bit integer.
     * @return Number of digits (i.e. length of digits past decimal in 1/amount)
     * Example:<br>
     * \verbatim￼
        xBridgeSignificantDigits(1000000)
        // returns 6
     * \endverbatim
     */
    unsigned int xBridgeSignificantDigits(int64_t amount);
     /** @brief makeError - generate standard json_sprit object with error description
     * @param statusCode - error code
     * @param function - nome of called function
     * @param message - additional error description
     * @return  json_spirit object with error description
     */
    Object makeError(const xbridge::Error statusCode, const std::string &function, const std::string &message = "");

} // namespace

#endif // UTIL_H
