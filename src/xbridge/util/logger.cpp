// Copyright (c) 2017-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//******************************************************************************
//******************************************************************************

#include <xbridge/util/logger.h>

#include <xbridge/xuiconnector.h>

#include <util/system.h>

#include <string>
#include <sstream>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>

boost::mutex logLocker;

//******************************************************************************
//******************************************************************************
// static
std::string LOG::m_logFileName;

//******************************************************************************
//******************************************************************************
LOG::LOG(const char reason)
    : std::basic_stringstream<char, std::char_traits<char>,
                    boost::pool_allocator<char> >()
    , m_r(reason)
{
    *this << "\n" << "[" << (char)std::toupper(m_r) << "] "
          << boost::posix_time::second_clock::local_time()
          << " [0x" << boost::this_thread::get_id() << "] ";
}

//******************************************************************************
//******************************************************************************
// static
std::string LOG::logFileName()
{
    return m_logFileName;
}

//******************************************************************************
//******************************************************************************
LOG::~LOG()
{
    boost::lock_guard<boost::mutex> lock(logLocker);

    try
    {
        static boost::gregorian::date day = boost::gregorian::day_clock::local_day();
        if (m_logFileName.empty())
            m_logFileName = makeFileName();

        boost::gregorian::date tmpday = boost::gregorian::day_clock::local_day();
        if (day != tmpday) {
            m_logFileName = makeFileName();
            day = tmpday;
        }

        std::ofstream file(m_logFileName.c_str(), std::ios_base::app);
        file << str().c_str();
    }
    catch (...) { }
}

//******************************************************************************
//******************************************************************************
std::string LOG::makeFileName()
{
    static boost::filesystem::path directory = GetDataDir(false) / "log";
    boost::filesystem::create_directory(directory);

    auto lt = boost::posix_time::second_clock::local_time();
    auto df = new boost::gregorian::date_facet("%Y%m%d");
    std::ostringstream ss;
    ss.imbue(std::locale(ss.getloc(), df));
    ss << lt.date();
    return directory.string() + "/" +
            "xbridgep2p_" + ss.str() + ".log";
}
