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
bool IniConfig::read(const char * fileName)
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

bool IniConfig::read(std::string config)
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
void XRouterSettings::loadPlugins()
{
    std::vector<std::string> plugins;
    std::string pstr = get<std::string>("Main.plugins", "");
    boost::split(plugins, pstr, boost::is_any_of(","));
    for(std::string s : plugins)
        loadPlugin(s);
}

void XRouterSettings::loadPlugin(std::string name)
{
    std::string filename = pluginPath() + name + ".conf";
    XRouterPluginSettings settings;
    if(!settings.read(filename.c_str()))
        return;
    this->plugins[name] = settings;
}

std::string XRouterSettings::pluginPath() const
{
    return std::string(GetDataDir(false).string()) + "/plugins/";
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

bool XRouterSettings::hasPlugin(std::string name)
{
    std::string type = get<std::string>(name + ".type", "");
    return type == "";
}

std::string XRouterSettings::getServiceParam(std::string name, std::string param, std::string def)
{
    return get<std::string>(name + "." + param, def);
}

double XRouterSettings::getServiceFee(std::string name)
{
    return get<double>(name + ".fee", 0.0);
}

int XRouterSettings::getServiceParamCount(std::string name)
{
    return get<int>(name + ".paramsCount", 0);
}

} // namespace xrouter