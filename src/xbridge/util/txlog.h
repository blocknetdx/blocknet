// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//******************************************************************************
//******************************************************************************

#ifndef BLOCKNET_XBRIDGE_UTIL_TXLOG_H
#define BLOCKNET_XBRIDGE_UTIL_TXLOG_H

#include <sstream>

#include <boost/pool/pool_alloc.hpp>

#define TXERR()   LOG('E')
// #define TXWARN()  LOG('W')
// #define TXTRACE() LOG('T')

//******************************************************************************
//******************************************************************************
class TXLOG : public std::basic_stringstream<char, std::char_traits<char>,
        boost::pool_allocator<char> > // std::stringstream
{
public:
    TXLOG();
    virtual ~TXLOG();

    static std::string logFileName();

private:
    static std::string makeFileName();

private:
    static std::string m_logFileName;
};

#endif // BLOCKNET_XBRIDGE_UTIL_TXLOG_H
