//******************************************************************************
//******************************************************************************

#include "xroutersettings.h"
#include "xrouterlogger.h"

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
    LOCK(mu);
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
        LOG() << e.what();
        return false;
    }

    return true;
}

bool IniConfig::read(std::string config)
{
    LOCK(mu);
    try
    {
        istringstream istr(config.c_str());
        boost::property_tree::ini_parser::read_ini(istr, m_pt);
        this->rawtext = config;
    }
    catch (std::exception & e)
    {
        LOG() << e.what();
        return false;
    }

    return true;
}

bool IniConfig::write(const char * fileName)
{
    LOCK(mu);
    std::string fname = m_fileName;
    try
    {
        if (fileName)
        {
            fname = std::string(fileName);
        }

        if (fname.empty())
        {
            return false;
        }

        boost::property_tree::ini_parser::write_ini(fname, m_pt);
        std::ostringstream oss;
        boost::property_tree::ini_parser::write_ini(oss, m_pt);
        this->rawtext = oss.str();
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
XRouterSettings::XRouterSettings(const std::string & config) {
    read(config);
    loadWallets();
    loadPlugins();
}

void XRouterSettings::loadPlugins()
{
    {
        LOCK(mu);
        plugins.clear();
        pluginList.clear();
    }
    std::vector<std::string> lplugins;
    std::string pstr = get<std::string>("Main.plugins", "");
    boost::split(lplugins, pstr, boost::is_any_of(","));

    for(std::string & s : lplugins)
        if(!s.empty() && loadPlugin(s)) {
            LOCK(mu);
            pluginList.insert(s);
        }
}

void XRouterSettings::loadWallets() {
    {
        LOCK(mu);
        wallets.clear();
    }
    std::vector<std::string> lwallets;
    std::string ws = get<std::string>("Main.wallets", "");
    boost::split(lwallets, ws, boost::is_any_of(","));
    for (const std::string & w : lwallets)
        if (!w.empty()) {
            LOCK(mu);
            wallets.insert(w);
        }
}

bool XRouterSettings::loadPlugin(std::string & name)
{
    std::string filename = pluginPath() + name + ".conf";
    auto settings = std::make_shared<XRouterPluginSettings>();
    LOG() << "Trying to load plugin " << name + ".conf";
    if(!settings->read(filename.c_str()))
        return false;

    LOCK(mu);
    this->plugins[name] = settings;
    LOG() << "Successfully loaded plugin " << name;
    return true;
}

std::string XRouterSettings::pluginPath() const
{
    return std::string(GetDataDir(false).string()) + "/plugins/";
}

bool XRouterSettings::walletEnabled(std::string & currency)
{
    LOCK(mu);
    return std::find(wallets.begin(), wallets.end(), currency) != wallets.end();
}

bool XRouterSettings::isAvailableCommand(XRouterCommand c, std::string currency)
{
    if (currency.empty())
        return false;
    if (!wallets.count(currency)) // check if currency supported
        return false;
    // Wallet commands are implicitly enabled until disabled
    auto disabled = get<bool>(currency + "::" + std::string(XRouterCommand_ToString(c)) + ".disabled", false);
    return !disabled;
}

double XRouterSettings::getCommandFee(XRouterCommand c, std::string currency, double def)
{
    auto res = get<double>("Main.fee", def);
    res = get<double>(std::string(XRouterCommand_ToString(c)) + ".fee", res);
    if (!currency.empty())
        res = get<double>(currency + "::" + std::string(XRouterCommand_ToString(c)) + ".fee", res);
    return res;
}

double XRouterSettings::getMaxFee(XRouterCommand c, std::string currency, double def)
{
    auto res = get<double>("Main.maxfee", def);
    res = get<double>(std::string(XRouterCommand_ToString(c)) + ".maxfee", res);
    if (!currency.empty())
        res = get<double>(currency + "::" + std::string(XRouterCommand_ToString(c)) + ".maxfee", res);
    return res;
}

int XRouterSettings::commandTimeout(XRouterCommand c, std::string currency, int def)
{
    auto res = get<int>("Main.timeout", def);
    res = get<int>(std::string(XRouterCommand_ToString(c)) + ".timeout", res);
    if (!currency.empty())
        res = get<int>(currency + "::" + std::string(XRouterCommand_ToString(c)) + ".timeout", res);
    return res;
}

int XRouterSettings::getCommandBlockLimit(XRouterCommand c, std::string currency, int def)
{
    auto res = get<int>("Main.blocklimit", def);
    res = get<int>(std::string(XRouterCommand_ToString(c)) + ".blocklimit", res);
    if (!currency.empty())
        res = get<int>(currency + "::" + std::string(XRouterCommand_ToString(c)) + ".blocklimit", res);
    return res;
}    

int XRouterSettings::clientRequestLimit(XRouterCommand c, std::string currency, int def) {
    auto res = get<int>("Main.clientrequestlimit", def);
    res = get<int>(std::string(XRouterCommand_ToString(c)) + ".clientrequestlimit", res);
    if (!currency.empty())
        res = get<int>(currency + "::" + std::string(XRouterCommand_ToString(c)) + ".clientrequestlimit", res);
    return res;
}

int XRouterSettings::configSyncTimeout()
{
    auto res = get<int>("Main.configsynctimeout", XROUTER_CONFIGSYNC_TIMEOUT);
    return res;
}

bool XRouterSettings::hasPlugin(std::string name)
{
    LOCK(mu);
    return plugins.count(name) > 0;
}

bool XRouterPluginSettings::read(const char * fileName)
{
    if (!IniConfig::read(fileName))
        return false;
    if (!verify(fileName))
        return false;
    
    formPublicText();
    return true;
}

bool XRouterPluginSettings::read(std::string config)
{
    if (!IniConfig::read(config))
        return false;
    if (!verify())
        return false;
    
    formPublicText();
    return true;
}

bool XRouterPluginSettings::verify(std::string name)
{
    LOCK(mu);
    bool result = true;
    std::string type;
    try {
        type = m_pt.get<std::string>("type");
        if ((type != "rpc") && (type != "shell")) {
            LOG() << "Can't load plugin " << name << ": unknown plugin type: " << type;
            result = false;
        }
    } catch (std::exception & e) {
        LOG() << "Can't load plugin " << name << ": type not specified";
        result = false;
    }
    
    int min_count = -1, max_count = -1;
    try {
        min_count = m_pt.get<int>("paramsCount");
        max_count = min_count;
    } catch (std::exception & e) {
        try {
            min_count = m_pt.get<int>("minParamsCount");
            max_count = m_pt.get<int>("maxParamsCount");
        } catch (std::exception & e) {
            LOG() << "Can't load plugin " << name << ": paramsCount or min/max paramsCount not specified";
            result = false;
        }
    }
    
    if (type == "rpc") {
        try {
            std::string typestring = m_pt.get<std::string>("paramsType");
            int type_count = std::count(typestring.begin(), typestring.end(), ',') + 1;
            if (type_count != max_count) {
                LOG() << "Can't load plugin " << name << ": paramsType string countains less elements than maxParamsCount";
                result = false;
            }
        } catch (std::exception & e) {
            LOG() << "Can't load plugin " << name << ": paramsType not specified";
            result = false;
        }
    }

    return result;
}

void XRouterPluginSettings::formPublicText()
{
    LOCK(mu);
    std::vector<string> lines;
    boost::split(lines, this->rawtext, boost::is_any_of("\n"));
    this->publictext = "";
    std::string prefix = "private::";
    for (const std::string & line : lines) {
        if (line.compare(0, prefix.size(), prefix))
            this->publictext += line + "\n";
    }
}

std::string XRouterPluginSettings::getParam(std::string param, std::string def)
{
    try
    {
        LOCK(mu);
        return m_pt.get<std::string>(param);
    }
    catch (std::exception & e)
    {
        return get<std::string>("private::" + param, def);
    }
}

double XRouterPluginSettings::getFee()
{
    return get<double>("fee", 0.0);
}

int XRouterPluginSettings::minParamCount()
{
    int res = get<int>("minParamsCount", -1);
    if (res < 0)
        res = get<int>("paramsCount", 0);
    return res;
}

int XRouterPluginSettings::maxParamCount()
{
    int res = get<int>("maxParamsCount", -1);
    if (res < 0)
        res = get<int>("paramsCount", 0);
    return res;
}

int XRouterPluginSettings::clientRequestLimit() {
    int res = get<int>("clientrequestlimit", -1);
    return res;
}

int XRouterPluginSettings::commandTimeout() {
    int res = get<int>("timeout", 30);
    return res;
}

} // namespace xrouter
