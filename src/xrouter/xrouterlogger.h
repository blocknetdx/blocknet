// Copyright (c) 2018-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_XROUTER_XROUTERLOGGER_H
#define BLOCKNET_XROUTER_XROUTERLOGGER_H

#include <sstream>

#include <boost/pool/pool_alloc.hpp>

#define WARN()  LOG('W')
#define ERR()   LOG('E')
#define TRACE() LOG('T')
#define TESTLOG() LOG('I',"test_xrouter.log")
#define DEBUGLOG() LOG('D')

#define LOG_KEYPAIR_VALUES

//******************************************************************************
//******************************************************************************
namespace xrouter
{
    
class LOG : public std::basic_stringstream<char, std::char_traits<char>,
                                        boost::pool_allocator<char> > // std::stringstream
{
public:
    explicit LOG(const char reason = 'I', std::string filename="");
    ~LOG() override;

    static std::string logFileName();

private:
    static std::string makeFileName();

private:
    char m_r;

    static std::string m_logFileName;
    std::string filenameOverride;
};

}

#endif // LOGGER_H
