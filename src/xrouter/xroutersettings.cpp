//******************************************************************************
//******************************************************************************

#include "xroutersettings.h"
#include "xrouterlogger.h"
#include "xroutererror.h"

#include "main.h" // GetDataDir

#include <algorithm>
#include <iostream>

#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>

namespace xrouter
{

static std::set<std::string> acceptableParameterTypes{"string","bool","int","double"};
static const std::string privatePrefix{"private::"};

//******************************************************************************
//******************************************************************************
bool IniConfig::read(const boost::filesystem::path & fileName)
{
    WaitableLock l(mu);
    try
    {
        if (!fileName.string().empty())
        {
            m_fileName = fileName.string();
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

bool IniConfig::read(const std::string & config)
{
    WaitableLock l(mu);
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
    WaitableLock l(mu);
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

XRouterSettings::XRouterSettings(const bool & ismine) : ismine(ismine) { }

bool XRouterSettings::init(const boost::filesystem::path & configPath) {
    if (!read(configPath)) {
        ERR() << "Failed to read xrouter config " << configPath.string();
        return false;
    }
    loadPlugins();
    loadWallets();
    return true;
}

bool XRouterSettings::init(const std::string & config) {
    if (!read(config)) {
        ERR() << "Failed to read xrouter config " << config;
        return false;
    }
    loadPlugins();
    loadWallets();
    return true;
}

void XRouterSettings::loadWallets() {
    {
        WaitableLock l(mu);
        wallets.clear();
    }
    std::vector<std::string> lwallets;
    std::string ws = get<std::string>("Main.wallets", "");
    boost::split(lwallets, ws, boost::is_any_of(","));
    for (const std::string & w : lwallets)
        if (!w.empty()) {
            WaitableLock l(mu);
            wallets.insert(w);
        }
}

void XRouterSettings::loadPlugins()
{
    {
        WaitableLock l(mu);
        plugins.clear();
        pluginList.clear();
    }
    std::vector<std::string> lplugins;
    std::string pstr = get<std::string>("Main.plugins", "");
    boost::split(lplugins, pstr, boost::is_any_of(","));

    for(std::string & s : lplugins)
        if(!s.empty() && loadPlugin(s)) {
            WaitableLock l(mu);
            pluginList.insert(s);
        }
}

bool XRouterSettings::hasPlugin(const std::string & name)
{
    WaitableLock l(mu);
    return plugins.count(name) > 0;
}

bool XRouterSettings::hasWallet(const std::string & currency)
{
    WaitableLock l(mu);
    return wallets.count(currency) > 0;
}

void XRouterSettings::defaultPaymentAddress(const std::string & paymentAddress) {
    {
        WaitableLock l(mu);
        if (!ismine)
            return;
    }

    const std::string s_paymentaddress{"paymentaddress"};
    const auto s_mainpaymentaddress = "Main."+s_paymentaddress;
    const auto & address = get<std::string>(s_mainpaymentaddress, "");
    if (address.empty() && !paymentAddress.empty())
        this->set<std::string>(s_mainpaymentaddress, paymentAddress); // assign the new payment address to the config
}

bool XRouterSettings::isAvailableCommand(XRouterCommand c, std::string service)
{
    // Handle plugin
    if (c == xrService)
        return hasPlugin(service);

    // XRouter command...
    if (service.empty())
        return false;
    if (!hasWallet(service)) // check if wallet supported
        return false;
    // Wallet commands are implicitly enabled until disabled
    auto disabled = get<bool>(service + "::" + std::string(XRouterCommand_ToString(c)) + ".disabled", false);
    return !disabled;
}

double XRouterSettings::maxFee(XRouterCommand c, std::string service, double def)
{
    const std::string cstr{XRouterCommand_ToString(c)};
    auto res = get<double>("Main.maxfee", def);

    if (c == xrService) { // Handle plugin
        if (!service.empty())
            res = get<double>(cstr + "::" + service + ".maxfee", res);
    } else {
        res = get<double>(cstr + ".maxfee", res);
        if (!service.empty())
            res = get<double>(service + "::" + cstr + ".maxfee", res);
    }

    return res;
}

int XRouterSettings::commandTimeout(XRouterCommand c, std::string service, int def)
{
    const std::string cstr{XRouterCommand_ToString(c)};
    auto res = get<int>("Main.timeout", def);

    if (c == xrService) { // Handle plugin
        if (!service.empty())
            res = get<int>(cstr + "::" + service + ".timeout", res);
    } else {
        res = get<int>(cstr + ".timeout", res);
        if (!service.empty())
            res = get<int>(service + "::" + cstr + ".timeout", res);
    }

    return res;
}

int XRouterSettings::confirmations(XRouterCommand c, std::string service, int def) {
    if (def > 1) // user requested consensus takes precedence
        return def;
    def = std::max(def, 1); // default must be at least 1 confirmation

    const std::string cstr{XRouterCommand_ToString(c)};
    auto res = get<int>("Main.consensus", def);

    if (c == xrService) { // Handle plugin
        if (!service.empty())
            res = get<int>(cstr + "::" + service + ".consensus", res);
    } else {
        res = get<int>(cstr + ".consensus", res);
        if (!service.empty())
            res = get<int>(service + "::" + cstr + ".consensus", res);
    }

    return res;
}

double XRouterSettings::defaultFee() {
    return get<double>("Main.fee", 0);
}

double XRouterSettings::commandFee(XRouterCommand c, std::string service, double def)
{
    // Handle plugin
    if (c == xrService && hasPlugin(service)) {
        auto ps = getPluginSettings(service);
        if (ps->has("fee"))
            return ps->fee();
        else
            return get<double>("Main.fee", def);
    }

    auto res = get<double>("Main.fee", def);
    res = get<double>(std::string(XRouterCommand_ToString(c)) + ".fee", res);
    if (!service.empty())
        res = get<double>(service + "::" + std::string(XRouterCommand_ToString(c)) + ".fee", res);
    return res;
}

int XRouterSettings::commandBlockLimit(XRouterCommand c, std::string currency, int def)
{
    auto res = get<int>("Main.blocklimit", def);
    res = get<int>(std::string(XRouterCommand_ToString(c)) + ".blocklimit", res);
    if (!currency.empty())
        res = get<int>(currency + "::" + std::string(XRouterCommand_ToString(c)) + ".blocklimit", res);
    return res;
}    

int XRouterSettings::clientRequestLimit(XRouterCommand c, std::string service, int def) {
    // Handle plugin
    if (c == xrService && hasPlugin(service)) {
        auto ps = getPluginSettings(service);
        if (ps->has("clientrequestlimit"))
            return ps->clientRequestLimit();
        else
            return get<double>("Main.clientrequestlimit", def);
    }

    auto res = get<int>("Main.clientrequestlimit", def);
    res = get<int>(std::string(XRouterCommand_ToString(c)) + ".clientrequestlimit", res);
    if (!service.empty())
        res = get<int>(service + "::" + std::string(XRouterCommand_ToString(c)) + ".clientrequestlimit", res);
    return res;
}

std::string XRouterSettings::paymentAddress(XRouterCommand c, const std::string service) {
    std::string def;
    static const auto s_paymentaddress = "paymentaddress";
    static const auto s_mainpaymentaddress = "Main.paymentaddress";

    // Handle plugin
    if (c == xrService && hasPlugin(service)) {
        auto ps = getPluginSettings(service);
        if (ps->has(s_paymentaddress) && !ps->paymentAddress().empty())
            return ps->paymentAddress();
        else
            return get<std::string>(s_mainpaymentaddress, def);
    }

    auto res = get<std::string>(s_mainpaymentaddress, def);
    res = get<std::string>(std::string(XRouterCommand_ToString(c)) + "." + s_paymentaddress, res);
    if (!service.empty())
        res = get<std::string>(service + "::" + std::string(XRouterCommand_ToString(c)) + "." + s_paymentaddress, res);
    return res;
}

int XRouterSettings::configSyncTimeout()
{
    auto res = get<int>("Main.configsynctimeout", XROUTER_CONFIGSYNC_TIMEOUT);
    return res;
}

std::map<std::string, double> XRouterSettings::feeSchedule() {

    double fee = defaultFee();
    std::map<std::string, double> s;

    WaitableLock l(mu);

    // First pass set top-level fees
    for (const auto & p : m_pt) {
        std::vector<std::string> parts;
        boost::split(parts, p.first, boost::is_any_of("::"));
        std::string cmd = parts[0];

        if (parts.size() > 1)
            continue; // skip currency fees (addressed in 2nd pass below)
        if (boost::algorithm::to_lower_copy(cmd) == "main")
            continue; // skip Main

        s[p.first] = m_pt.get<double>(p.first + ".fee", fee);
    }

    // 2nd pass to set currency fees
    for (const auto & p : m_pt) {
        if (s.count(p.first))
            continue; // skip existing

        std::vector<std::string> parts;
        boost::split(parts, p.first, boost::is_any_of("::"));

        if (parts.size() < 3)
            continue; // skip

        std::string currency = parts[0];
        std::string cmd = parts[2];

        s[p.first] = m_pt.get<double>(p.first + ".fee", s.count(cmd) ? s[cmd] : fee); // default to top-level fee
    }

    return s;
}

bool XRouterSettings::loadPlugin(const std::string & name)
{
    if (!ismine) // only load our own configs
        return true;

    auto conf = name + ".conf";
    auto filename = pluginPath() / conf;
    auto settings = std::make_shared<XRouterPluginSettings>();

    if(!settings->read(filename)) {
        LOG() << "Failed to load plugin: " << conf;
        return false;
    }
    LOG() << "Successfully loaded plugin " << name;

    WaitableLock l(mu);
    plugins[name] = settings;
    return true;
}

boost::filesystem::path XRouterSettings::pluginPath() const
{
    return GetDataDir(false) / "plugins";
}

///////////////////////////////////
///////////////////////////////////
///////////////////////////////////

bool XRouterPluginSettings::read(const boost::filesystem::path & fileName)
{
    if (!IniConfig::read(fileName))
        return false;
    if (!verify(fileName.string()))
        return false;
    
    formPublicText();
    return true;
}

bool XRouterPluginSettings::read(const std::string & config)
{
    if (!IniConfig::read(config))
        return false;
    if (!verify(config))
        return false;
    
    formPublicText();
    return true;
}

bool XRouterPluginSettings::verify(const std::string & name)
{
    bool result{true};

    try {
        // Make sure specified type is one of the acceptable types
        const auto & params = parameters();
        for (const auto & p : params) {
            if (!acceptableParameterTypes.count(p)) {
                LOG() << "Unsupported parameter type " << p << " found in plugin config " << name;
                result = false;
            }
        }
    } catch (std::exception&) {
        LOG() << "Failed to load plugin " << name << " type not specified";
        result = false;
    }

    return result;
}

void XRouterPluginSettings::formPublicText()
{
    WaitableLock l(mu);

    publictext.clear(); // reset
    std::vector<string> lines;
    boost::split(lines, rawtext, boost::is_any_of("\n"));

    // Exclude commands with the private prefix
    for (const std::string & line : lines) {
        if (line.compare(0, privatePrefix.size(), privatePrefix))
            publictext += line + "\n";
    }
}

std::string XRouterPluginSettings::stringParam(const std::string & param, const std::string def) {
    const auto & t = get<std::string>(param, "");
    if (t.empty())
        return get<std::string>(privatePrefix + param, def);
    return t;
}

std::vector<std::string> XRouterPluginSettings::parameters() {
    std::vector<std::string> p;
    const auto & params = get<std::string>("parameters", "");
    if (params.empty())
        return p;
    boost::split(p, params, boost::is_any_of(","));
    return p;
}

std::string XRouterPluginSettings::type() {
    auto t = get<std::string>("type", "");
    if (t.empty())
       t = get<std::string>(privatePrefix + "type", "");
    if (t.empty())
        throw XRouterError("Missing type in plugin", INVALID_PARAMETERS);
    return t;
}

double XRouterPluginSettings::fee() {
    return get<double>("fee", 0.0);
}

int XRouterPluginSettings::clientRequestLimit() {
    int res = get<int>("clientrequestlimit", -1);
    return res;
}

int XRouterPluginSettings::commandTimeout() {
    int res = get<int>("timeout", 30);
    return res;
}

std::string XRouterPluginSettings::paymentAddress() {
    auto res = get<std::string>("paymentaddress", "");
    return res;
}

} // namespace xrouter
