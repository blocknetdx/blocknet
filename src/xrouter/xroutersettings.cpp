//******************************************************************************
//******************************************************************************

#include "xroutersettings.h"
//#include "../config.h"

#include "../main.h"

#include <algorithm>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>


//******************************************************************************
//******************************************************************************
XRouterSettings::XRouterSettings()
    : m_isExchangeEnabled(false)
{
}

//******************************************************************************
//******************************************************************************
bool XRouterSettings::parseCmdLine(int, char * argv[])
{
    m_appPath = std::string(argv[0]);
    std::replace(m_appPath.begin(), m_appPath.end(), '\\', '/');
    m_appPath = m_appPath.substr(0, m_appPath.rfind('/')+1);

    bool enableExchange = GetBoolArg("-enableexchange", false);

    if (enableExchange)
    {
        m_isExchangeEnabled = true;
        //LOG() << "exchange enabled by passing argument";
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool XRouterSettings::read(const char * fileName)
{
    try
    {
        if (fileName)
        {
            m_fileName = std::string(fileName);
        }

        if (m_fileName.empty())
        {
            return false;
        }

        boost::property_tree::ini_parser::read_ini(m_fileName, m_pt);
    }
    catch (std::exception & e)
    {
        //LOG() << e.what();
        return false;
    }

    return true;
}

//******************************************************************************
//******************************************************************************
std::string XRouterSettings::logPath() const
{
//    try { return m_pt.get<std::string>("Main.LogPath"); }
//    catch (std::exception &) {} return std::string();

    return std::string(GetDataDir(false).string()) + "/";
}