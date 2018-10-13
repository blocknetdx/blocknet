//******************************************************************************
//******************************************************************************

#include "settings.h"
#include "logger.h"
#include "../config.h"

#include "../../currency.h"
#include "../../main.h"

#include <algorithm>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>

//******************************************************************************
//******************************************************************************
Settings & settings()
{
    static Settings s;
    return s;
}

//******************************************************************************
//******************************************************************************
Settings::Settings()
    : m_isExchangeEnabled(false)
{
}

//******************************************************************************
//******************************************************************************
bool Settings::parseCmdLine(int, char * argv[])
{
    LOCK(m_lock);
    m_appPath = std::string(argv[0]);
    std::replace(m_appPath.begin(), m_appPath.end(), '\\', '/');
    m_appPath = m_appPath.substr(0, m_appPath.rfind('/')+1);

    bool enableExchange = GetBoolArg("-enableexchange", false);

    if (enableExchange)
    {
        m_isExchangeEnabled = true;
        LOG() << "exchange enabled by passing argument";
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool Settings::read(const char * fileName)
{
    LOCK(m_lock);
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
        LOG() << e.what();
        return false;
    }

    return true;
}

//******************************************************************************
//******************************************************************************
bool Settings::write(const char * fileName)
{
    LOCK(m_lock);
    try
    {
        std::string iniName = m_fileName;
        if (fileName)
        {
            iniName = std::string(fileName);
        }

        if (iniName.empty())
        {
            return false;
        }

        boost::property_tree::ini_parser::write_ini(iniName, m_pt);
    }
    catch (std::exception & e)
    {
        LOG() << e.what();
        return false;
    }

    return true;
}

//******************************************************************************
//******************************************************************************
std::string Settings::logPath() const
{
//    try { return m_pt.get<std::string>("Main.LogPath"); }
//    catch (std::exception &) {} return std::string();

    return std::string(GetDataDir(false).string()) + "/";
}

//******************************************************************************
//******************************************************************************
std::vector<std::string> Settings::peers() const
{
    LOCK(m_lock);
    std::string list;
    TRY(list = m_pt.get<std::string>("Main.Peers"));

    std::vector<std::string> strs;
    if (list.size() > 0)
    {
        boost::split(strs, list, boost::is_any_of(",;"));
    }

    std::copy(m_peers.begin(), m_peers.end(), std::back_inserter(strs));
    return strs;
}

//******************************************************************************
//******************************************************************************
std::vector<std::string> Settings::exchangeWallets() const
{
    LOCK(m_lock);
    std::string list;
    TRY(list = m_pt.get<std::string>("Main.ExchangeWallets"));

    std::vector<std::string> strs;
    if (list.size() > 0)
    {
        std::vector<std::string> raw_symbols;
        boost::split(raw_symbols, list, boost::is_any_of(",;:"));
        for (const auto& raw : raw_symbols) {
            std::string sym{ccy::Symbol::validate(raw)};
            if (sym.empty()) {
                // TODO warn and ignore
                continue;
            } else if (sym != raw) {
                // TODO warn and accept
            }
            strs.push_back(sym);
        }
    }

    return strs;
}
