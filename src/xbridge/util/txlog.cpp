//******************************************************************************
//******************************************************************************

#include "txlog.h"
#include "settings.h"
#include "xbridge/xuiconnector.h"

#include "util.h"

#include <string>
#include <sstream>
#include <fstream>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/filesystem.hpp>

boost::mutex txlogLocker;

//******************************************************************************
//******************************************************************************
// static
std::string TXLOG::m_logFileName;

//******************************************************************************
//******************************************************************************
TXLOG::TXLOG()
    : std::basic_stringstream<char, std::char_traits<char>,
                    boost::pool_allocator<char> >()
{
    *this << "\n"
          << boost::posix_time::second_clock::local_time()
          << " [0x" << boost::this_thread::get_id() << "] ";
}

//******************************************************************************
//******************************************************************************
// static
std::string TXLOG::logFileName()
{
    return m_logFileName;
}

//******************************************************************************
//******************************************************************************
TXLOG::~TXLOG()
{
    boost::mutex::scoped_lock l(txlogLocker);

    // const static std::string path     = settings().logPath().size() ? settings().logPath() : settings().appPath();
    const static bool logToFile       = true; // !path.empty();
    static boost::gregorian::date day =
            boost::gregorian::day_clock::local_day();
    if (m_logFileName.empty())
    {
        m_logFileName    = makeFileName();
    }

    // std::cout << str().c_str();

    try
    {
        if (logToFile)
        {
            boost::gregorian::date tmpday =
                    boost::gregorian::day_clock::local_day();

            if (day != tmpday)
            {
                m_logFileName = makeFileName();
                day = tmpday;
            }

            std::ofstream file(m_logFileName.c_str(), std::ios_base::app);
            file << str().c_str();
        }
    }
    catch (std::exception &)
    {
    }
}

//******************************************************************************
//******************************************************************************
// static
std::string TXLOG::makeFileName()
{
    boost::filesystem::path directory = GetDataDir(false) / "log-tx";
    boost::filesystem::create_directory(directory);

    return directory.string() + "/" +
            "xbridgep2p_" +
            boost::posix_time::to_iso_string(boost::posix_time::second_clock::local_time()) +
            ".log";
}
