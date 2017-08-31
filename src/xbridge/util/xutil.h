//*****************************************************************************
//*****************************************************************************

#ifndef UTIL_H
#define UTIL_H

#include "uint256.h"
#include "logger.h"

#include <string>

//*****************************************************************************
//*****************************************************************************
namespace util
{
    void init();

    std::wstring wide_string(std::string const & s);//, std::locale const &loc);
    // std::string narrow_string(std::wstring const &s, char default_char = '?');//, std::locale const &loc, char default_char = '?');

    std::string mb_string(std::string const & s);
    std::string mb_string(std::wstring const & s);

    std::string base64_encode(const std::vector<unsigned char> & s);
    std::string base64_encode(const std::string & s);
    std::string base64_decode(const std::string & s);

    template<class _T> std::string to_str(const _T & obj)
    {
        return util::base64_encode(std::string((char *)(obj.begin()),
                                               (char *)(obj.end())));
    }

} // namespace

#endif // UTIL_H
