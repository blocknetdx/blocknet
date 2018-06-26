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
#include <iostream>

namespace xrouter
{  
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
        
        ifstream ifs(m_fileName.c_str(), ios::in | ios::binary | ios::ate);

        ifstream::pos_type fileSize = ifs.tellg();
        ifs.seekg(0, ios::beg);

        vector<char> bytes(fileSize);
        ifs.read(bytes.data(), fileSize);

        this->rawtext = string(bytes.data(), fileSize);
    }
    catch (std::exception & e)
    {
        //LOG() << e.what();
        return false;
    }

    return true;
}

bool XRouterSettings::read(std::string config)
{
    try
    {
        istringstream istr(config.c_str());
        boost::property_tree::ini_parser::read_ini(istr, m_pt);
        this->rawtext = config;
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

bool XRouterSettings::walletEnabled(std::string currency)
{
    std::vector<string> wallets;
    std::string wstr = get<std::string>("Main.wallets", "");
    boost::split(wallets, wstr, boost::is_any_of(","));
    if (std::find(wallets.begin(), wallets.end(), currency) != wallets.end())
        return true;
    else
        return false;
}

bool XRouterSettings::isAvailableCommand(XRouterCommand c, std::string currency, bool def)
{
    int res = 0;
    if (def)
        res = 1;
    res = get<int>(std::string(XRouterCommand_ToString(c)) + ".run", res);
    if (!currency.empty())
        res = get<int>(currency + "::" + std::string(XRouterCommand_ToString(c)) + ".run", res);
    if (res)
        return true;
    else
        return false;
}

double XRouterSettings::getCommandFee(XRouterCommand c, std::string currency, double def)
{
    double res = get<double>(std::string(XRouterCommand_ToString(c)) + ".fee", def);
    if (!currency.empty())
        res = get<double>(currency + "::" + std::string(XRouterCommand_ToString(c)) + ".fee", res);
    return res;
}

double XRouterSettings::getCommandTimeout(XRouterCommand c, std::string currency, double def)
{
    double res = get<double>("Main.timeout", def);
    res = get<double>(std::string(XRouterCommand_ToString(c)) + ".timeout", def);
    if (!currency.empty())
        res = get<double>(currency + "::" + std::string(XRouterCommand_ToString(c)) + ".timeout", res);
    return res;
}

} // namespace xrouter