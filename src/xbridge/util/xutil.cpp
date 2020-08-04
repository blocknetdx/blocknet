// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//*****************************************************************************
//*****************************************************************************

#include <xbridge/util/xutil.h>

#include <xbridge/xbridgetransactiondescr.h>

#include <amount.h>

#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

#include <openssl/rand.h>

#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/ostream_iterator.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/time_facet.hpp>
#include <boost/locale.hpp>
#include <boost/numeric/conversion/cast.hpp>

#ifndef WIN32
#include <execinfo.h>
#endif

//*****************************************************************************
//*****************************************************************************
namespace xbridge
{

using namespace json_spirit;
std::locale loc;

//******************************************************************************
//******************************************************************************
void init()
{
    try
    {
        loc = std::locale ("en_US.UTF8");
    }
    catch (std::runtime_error & e)
    {
        LOG() << "use default locale, " << e.what();
        loc = std::locale (loc, "", std::locale::ctype);
    }
}

//******************************************************************************
//******************************************************************************
std::wstring wide_string(std::string const &s)//, std::locale const &loc)
{
    if (s.empty())
    {
        return std::wstring();
    }

    std::ctype<wchar_t> const &facet = std::use_facet<std::ctype<wchar_t> >(loc);
    char const *first = s.c_str();
    char const *last = first + s.size();
    std::vector<wchar_t> result(s.size());

    facet.widen(first, last, &result[0]);

    return std::wstring(result.begin(), result.end());
}

//******************************************************************************
//******************************************************************************
//std::string narrow_string(std::wstring const &s, char default_char)//, std::locale const &loc, char default_char)
//{
//    if (s.empty())
//    {
//        return std::string();
//    }

//    std::ctype<wchar_t> const &facet = std::use_facet<std::ctype<wchar_t> >(loc);
//    wchar_t const *first = s.c_str();
//    wchar_t const *last = first + s.size();
//    std::vector<char> result(s.size());

//    facet.narrow(first, last, default_char, &result[0]);

//    return std::string(result.begin(), result.end());
//}

//******************************************************************************
//******************************************************************************
std::string mb_string(std::string const &s)
{
    return mb_string(wide_string(s));
}

//******************************************************************************
//******************************************************************************
std::string mb_string(std::wstring const &s)
{
    return boost::locale::conv::utf_to_utf<char>(s);
}

//*****************************************************************************
//*****************************************************************************
const std::string base64_padding[] = {"", "==","="};

//*****************************************************************************
//*****************************************************************************
std::string base64_encode(const std::vector<unsigned char> & s)
{
    return base64_encode(std::string((char *)&s[0], s.size()));
}

//*****************************************************************************
//*****************************************************************************
std::string base64_encode(const std::string& s)
{
    namespace bai = boost::archive::iterators;

    std::stringstream os;

    // convert binary values to base64 characters
    typedef bai::base64_from_binary
    // retrieve 6 bit integers from a sequence of 8 bit bytes
    <bai::transform_width<const char *, 6, 8> > base64_enc; // compose all the above operations in to a new iterator

    std::copy(base64_enc(s.c_str()), base64_enc(s.c_str() + s.size()),
            std::ostream_iterator<char>(os));

    os << base64_padding[s.size() % 3];
    return os.str();
}

//*****************************************************************************
//*****************************************************************************
std::string base64_decode(const std::string& s)
{
    try
    {
        namespace bai = boost::archive::iterators;

        std::stringstream os;

        typedef bai::transform_width<bai::binary_from_base64<const char *>, 8, 6> base64_dec;

        unsigned int size = s.size();

        // Remove the padding characters, cf. https://svn.boost.org/trac/boost/ticket/5629
        if (size && s[size - 1] == '=')
        {
            --size;
            if (size && s[size - 1] == '=')
            {
                --size;
            }
        }
        if (size == 0)
        {
            return std::string();
        }

        std::copy(base64_dec(s.data()), base64_dec(s.data() + size),
                std::ostream_iterator<char>(os));

        return os.str();
    }
    // catch (std::exception &)
    catch (...)
    {
    }
    return std::string();
}

std::string to_str(const std::vector<unsigned char> & obj)
{
    return base64_encode(obj);
}

std::string iso8601(const boost::posix_time::ptime &time)
{
    auto ms = time.time_of_day().total_milliseconds() % 1000;
    auto tm = to_tm(time);
    std::ostringstream ss;
#if __GNUC__ < 5
    char buf[sizeof "2019-12-15T12:00:00"];
    strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%S", &tm);
    ss << std::string(buf);
#else
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
#endif
    ss << '.' << std::setfill('0') << std::setw(3) << ms; // add milliseconds
    ss << 'Z';
    return ss.str();
}

std::string xBridgeStringValueFromAmount(CAmount amount)
{
    std::stringstream ss;
    ss << std::fixed << std::setprecision(xBridgeSignificantDigits(xbridge::TransactionDescr::COIN)) << xBridgeValueFromAmount(amount);
    return ss.str();
}

std::string xBridgeStringValueFromPrice(double price)
{
    std::stringstream ss;
    ss << std::fixed << std::setprecision(xBridgeSignificantDigits(xbridge::TransactionDescr::COIN)) << price;
    return ss.str();
}

std::string xBridgeStringValueFromPrice(double price, uint64_t denomination)
{
    std::stringstream ss;
    ss << std::fixed << std::setprecision(xBridgeSignificantDigits(denomination)) << price;
    return ss.str();
}

double xBridgeValueFromAmount(CAmount amount) {
    return static_cast<double>(amount)
           / static_cast<double>(xbridge::TransactionDescr::COIN)
           + 1.0 / static_cast<double>(::COIN); // round up 1 sat
}

/**
 * Does not round, but truncates because a utxo cannot pay if it's rounded up
 */
CAmount xBridgeIntFromReal(double utxo_amount) {
    double d = utxo_amount * boost::numeric_cast<double>(xbridge::TransactionDescr::COIN);
    d += 1.0 / static_cast<double>(::COIN); // round up 1 sat
    auto r = static_cast<CAmount>(d);
    return r;
}

CAmount xBridgeAmountFromReal(double val) {
    return xBridgeIntFromReal(val);
}

bool xBridgeValidCoin(const std::string coin)
{
    bool f = false;
    int n = 0;
    int j = 0; // count 0s
    // count precision digits, ignore trailing 0s
    for (const char &c : coin) {
        if (!f && c == '.')
            f = true;
        else if (f) {
            n++;
            if (c == '0')
                j++;
            else
                j = 0;
        }
    }
    return n - j <= xBridgeSignificantDigits(xbridge::TransactionDescr::COIN);
}

unsigned int xBridgeSignificantDigits(const int64_t amount)
{
    unsigned int n = 0;
    int64_t i = amount;

    do {
        n++;
        i /= 10;
    } while (i > 1);

    return n;
}

uint64_t timeToInt(const boost::posix_time::ptime& time)
{
    boost::posix_time::ptime start(boost::gregorian::date(1970,1,1));
    boost::posix_time::time_duration timeFromEpoch = time - start;
    boost::int64_t res = timeFromEpoch.total_microseconds();

    return static_cast<uint64_t>(res);
}

boost::posix_time::ptime intToTime(const uint64_t& number)
{
    boost::posix_time::ptime start(boost::gregorian::date(1970,1,1));
    boost::posix_time::ptime res = start + boost::posix_time::microseconds(static_cast<int64_t>(number));

    return res;
}

double price(const xbridge::TransactionDescrPtr ptr)
{
    if(ptr == nullptr) {
        return .0;
    }
    if(fabs(ptr->fromAmount)  < std::numeric_limits<double>::epsilon()) {
        return  .0;
    }
    return xBridgeValueFromAmount(ptr->toAmount) / xBridgeValueFromAmount(ptr->fromAmount);
}
double priceBid(const xbridge::TransactionDescrPtr ptr)
{
    if(ptr == nullptr) {
        return .0;
    }
    if(fabs(ptr->toAmount)  < std::numeric_limits<double>::epsilon()) {
        return  .0;
    }
    return xBridgeValueFromAmount(ptr->fromAmount) / xBridgeValueFromAmount(ptr->toAmount);
}

CAmount xBridgeDestAmountFromPrice(const CAmount counterpartySourceAmount, const CAmount sourceAmount, const CAmount destAmount) {
    static CAmount c = 1000000;
    const auto csa = counterpartySourceAmount * c;
    const auto sa = sourceAmount * c;
    const auto da = destAmount * c;
    CAmount newDestAmount = csa * (static_cast<double>(da) / static_cast<double>(sa)) + 1;
    newDestAmount /= c; // normalize
    if (newDestAmount < 1)
        return 1;
    return newDestAmount;
}

CAmount xBridgeSourceAmountFromPrice(const CAmount counterpartyDestAmount, const CAmount sourceAmount, const CAmount destAmount) {
    static CAmount c = 1000000;
    const auto cda = counterpartyDestAmount * c;
    const auto sa = sourceAmount * c;
    const auto da = destAmount * c;
    CAmount newSourceAmount = cda * (static_cast<double>(sa) / static_cast<double>(da)) + 1;
    newSourceAmount /= c; // normalize
    if (newSourceAmount < 1)
        return 1;
    return newSourceAmount;
}

bool xBridgePartialOrderDriftCheck(CAmount makerSource, CAmount makerDest, CAmount otherSource, CAmount otherDest) {
    bool success{true}; // error
    // Exact order should always succeed
    if (makerSource == otherDest && makerDest == otherSource)
        return true;
    // Taker amounts must agree with maker's asking price. Derive asking amounts from
    // counterparty provided amounts. By deriving these amounts from the counterparty
    // we can ensure price integrity.
    const CAmount checkSourceAmount = xBridgeSourceAmountFromPrice(makerDest, otherDest, otherSource);
    const CAmount checkDestAmount = xBridgeDestAmountFromPrice(makerSource, otherDest, otherSource);
    const CAmount checkSourceAmountOther = xBridgeSourceAmountFromPrice(otherDest, makerDest, makerSource);
    const CAmount checkDestAmountOther = xBridgeDestAmountFromPrice(otherSource, makerDest, makerSource);
    // Price match check. The type of check changes based on whether there is a remainder when
    // checking if the maker's total order amounts are divisible by the counterparty's partial
    // order amounts. If the amounts are divisible then we expect an exact match. If the amounts
    // are not divisible then we use a drift check on the smallest amount using an upper and
    // lower bound check. This means the forgiveness could be anywhere from 100 sats to 1000 sats
    // or more as the partially taken order sizes decrease in size.
    if (makerSource % otherDest == 0 && makerDest % otherSource == 0) {
        if (checkSourceAmountOther != otherSource
            || checkDestAmountOther != otherDest
            || checkSourceAmount != makerSource
            || checkDestAmount != makerDest)
            success = false;
    } else if (checkSourceAmountOther != otherSource
               || checkDestAmountOther != otherDest
               || checkSourceAmount != makerSource
               || checkDestAmount != makerDest)
    {
        const CAmount driftTakerSourceA = xBridgeSourceAmountFromPrice(otherDest + 1, makerDest, makerSource);
        const CAmount driftTakerSourceB = xBridgeSourceAmountFromPrice(otherDest - 1, makerDest, makerSource);
        const CAmount driftTakerSourceUpper = driftTakerSourceA > driftTakerSourceB ? driftTakerSourceA : driftTakerSourceB;
        const CAmount driftTakerSourceLower = driftTakerSourceA < driftTakerSourceB ? driftTakerSourceA : driftTakerSourceB;
        if (otherSource > driftTakerSourceUpper || otherSource < driftTakerSourceLower)
            success = false;
        const CAmount driftTakerDestA = xBridgeDestAmountFromPrice(otherSource + 1, makerDest, makerSource);
        const CAmount driftTakerDestB = xBridgeDestAmountFromPrice(otherSource - 1, makerDest, makerSource);
        const CAmount driftTakerDestUpper = driftTakerDestA > driftTakerDestB ? driftTakerDestA : driftTakerDestB;
        const CAmount driftTakerDestLower = driftTakerDestA < driftTakerDestB ? driftTakerDestA : driftTakerDestB;
        if (otherDest > driftTakerDestUpper || otherDest < driftTakerDestLower)
            success = false;
    }

    return success;
}

json_spirit::Object makeError(const xbridge::Error statusCode, const std::string &function, const std::string &message)
{
    Object error;
    error.emplace_back(Pair("error",xbridge::xbridgeErrorText(statusCode,message)));
    error.emplace_back(Pair("code", statusCode));
    error.emplace_back(Pair("name",function));
    return  error;
}

void LogOrderMsg(const std::string & orderId, const std::string & msg, const std::string & func) {
    UniValue o(UniValue::VOBJ);
    o.pushKV("orderid", orderId);
    o.pushKV("function", func);
    o.pushKV("msg", msg);
    LOG() << o.write();
}
void LogOrderMsg(UniValue o, const std::string & msg, const std::string & func) {
    o.pushKV("function", func);
    o.pushKV("msg", msg);
    LOG() << o.write();
}
void LogOrderMsg(xbridge::TransactionDescrPtr & ptr, const std::string & func) {
    LOG() << func << " " << ptr;
}
void LogOrderMsg(xbridge::TransactionPtr & ptr, const std::string & func) {
    LOG() << func << " " << ptr;
}

} // namespace xbridge