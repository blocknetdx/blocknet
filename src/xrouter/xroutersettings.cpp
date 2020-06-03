// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <xrouter/xroutersettings.h>

#include <xrouter/xroutererror.h>
#include <xrouter/xrouterlogger.h>
#include <xrouter/xrouterutils.h>

#include <chainparams.h>
#include <key_io.h>
#include <netbase.h>
#include <util/system.h>

#include <algorithm>
#include <iostream>
#include <regex>

#include <boost/algorithm/string.hpp>

namespace sn {
extern CTxDestination ServiceNodePaymentAddress(const std::string & snode);
}

namespace xrouter
{

static std::set<std::string> acceptableParameterTypes{"string","bool","int","double"};
static const std::string privatePrefix{"private::"};
static const std::string privateComment{"#!"};

static int maxFetchLimit(const int & fl) {
    if (fl < 0)
        return std::numeric_limits<int>::max();
    else return fl;
};

//******************************************************************************
//******************************************************************************
bool IniConfig::read(const boost::filesystem::path & fileName)
{
    try
    {
        if (!fileName.string().empty())
            m_fileName = fileName.string();

        if (m_fileName.empty())
            return false;

        {
            LOCK(mu);
            boost::property_tree::ini_parser::read_ini(m_fileName, m_pt);
            std::ifstream ifs(m_fileName.c_str(), std::ios::in | std::ios::binary | std::ios::ate);
            std::ifstream::pos_type fileSize = ifs.tellg();
            ifs.seekg(0, std::ios::beg);
            std::vector<char> bytes(fileSize);
            ifs.read(bytes.data(), fileSize);
            rawtext = std::string(bytes.data(), fileSize);
        }
        genPublic();
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
    try
    {
        {
            LOCK(mu);
            std::istringstream istr(config.c_str());
            boost::property_tree::ini_parser::read_ini(istr, m_pt);
            rawtext = config;
        }
        genPublic();
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
    try
    {
        std::string fname = m_fileName;
        if (fileName)
            fname = std::string(fileName);

        if (fname.empty())
            return false;

        {
            LOCK(mu);
            boost::property_tree::ini_parser::write_ini(fname, m_pt);
            std::ostringstream oss;
            boost::property_tree::ini_parser::write_ini(oss, m_pt);
            rawtext = oss.str();
        }
        genPublic();
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

XRouterSettings::XRouterSettings(const CPubKey & pubkey, const bool & ismine) : snodePubKey(pubkey), ismine(ismine) { }

bool XRouterSettings::init(const boost::filesystem::path & configPath, const bool snode) {
    if (!read(configPath)) {
        ERR() << "Failed to read xrouter config " << configPath.string();
        return false;
    }
    if (snode && host(xrDefault).empty()) {
        ERR() << "Failed to read xrouter config, missing \"host\" entry " << configPath.string();
        return false;
    }
    if (snode) {
        CNetAddr caddr;
        if (!LookupHost(host(xrDefault).c_str(), caddr, true)) {
            ERR() << "Failed to read xrouter config, bad \"host\" entry " << configPath.string();
            return false;
        }
        const auto nport = port(xrDefault);
        addr = CService(caddr, nport);
        node = host(xrDefault) + ":" + std::to_string(nport);
    }
    loadPlugins();
    loadWallets();
    return true;
}

bool XRouterSettings::init(const std::string & config, const bool snode) {
    if (!read(config)) {
        ERR() << "Failed to read xrouter config " << config;
        return false;
    }
    if (snode && host(xrDefault).empty()) {
//        ERR() << "Failed to read xrouter config, missing \"host\" entry " << config;
        return false;
    }
    if (snode) {
        CNetAddr caddr;
        if (!LookupHost(host(xrDefault).c_str(), caddr, true)) {
//            ERR() << "Failed to read xrouter config, bad \"host\" entry " << config;
            return false;
        }
        const auto nport = port(xrDefault);
        addr = CService(caddr, nport);
        node = host(xrDefault) + ":" + std::to_string(nport);
    }
    loadPlugins();
    loadWallets();
    return true;
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

bool XRouterSettings::hasPlugin(const std::string & name)
{
    LOCK(mu);
    return plugins.count(name) > 0;
}

bool XRouterSettings::hasWallet(const std::string & currency)
{
    LOCK(mu);
    return wallets.count(currency) > 0;
}

void XRouterSettings::defaultPaymentAddress(const std::string & paymentAddress) {
    {
        LOCK(mu);
        if (!ismine)
            return;
    }

    const std::string s_paymentaddress{"paymentaddress"};
    const auto s_mainpaymentaddress = "Main."+s_paymentaddress;
    const auto & address = get<std::string>(s_mainpaymentaddress, "");
    if (address.empty() && !paymentAddress.empty())
        this->set<std::string>(s_mainpaymentaddress, paymentAddress); // assign the new payment address to the config
}

bool XRouterSettings::isAvailableCommand(XRouterCommand c, const std::string & service)
{
    // Handle plugin
    if (c == xrService) {
        if (!hasPlugin(service))
            return false;
        auto ps = getPluginSettings(service); // check if plugin is disabled
        if (ps)
            return !ps->disabled();
        return false; // something's wrong with the plugin, report that it's disabled
    }

    // XRouter command...
    if (service.empty())
        return false;
    if (!hasWallet(service)) // check if wallet supported
        return false;
    // Wallet commands are implicitly enabled until disabled
    auto disabled = get<bool>(service + xrdelimiter + std::string(XRouterCommand_ToString(c)) + ".disabled", false);
    return !disabled;
}

std::string XRouterSettings::host(XRouterCommand c, const std::string & service) {
    const std::string cstr{XRouterCommand_ToString(c)};
    auto res = get<std::string>("Main.host", "");

    // TODO Blocknet XRouter support subsection host designations
//    if (c == xrService) { // Handle plugin
//        if (!service.empty())
//            res = get<std::string>(cstr + xrdelimiter + service + ".host", res);
//    } else if (c != xrDefault) {
//        res = get<std::string>(cstr + ".host", res);
//        if (!service.empty()) {
//            res = get<std::string>(service + ".host", res);
//            res = get<std::string>(service + xrdelimiter + cstr + ".host", res);
//        }
//    }

    return res;
}

int XRouterSettings::port(XRouterCommand c, const std::string & service) {
    const std::string cstr{XRouterCommand_ToString(c)};
    auto res = get<int>("Main.port", Params().GetDefaultPort());

    // TODO Blocknet XRouter support subsection port designations
//    if (c == xrService) { // Handle plugin
//        if (!service.empty())
//            res = get<int>(cstr + xrdelimiter + service + ".port", res);
//    } else if (c != xrDefault) {
//        res = get<int>(cstr + ".port", res);
//        if (!service.empty()) {
//            res = get<int>(service + ".port", res);
//            res = get<int>(service + xrdelimiter + cstr + ".port", res);
//        }
//    }

    return res;
}

bool XRouterSettings::tls(XRouterCommand c, const std::string & service) {
    auto res = get<bool>("Main.tls", false);

    // TODO Blocknet XRouter support subsection tls designations

    return res;
}

double XRouterSettings::maxFee(XRouterCommand c, const std::string & service, double def)
{
    const std::string cstr{XRouterCommand_ToString(c)};
    auto res = get<double>("Main.maxfee", def);

    if (c == xrService) { // Handle plugin
        if (!service.empty())
            res = get<double>(cstr + xrdelimiter + service + ".maxfee", res);
    } else {
        res = get<double>(cstr + ".maxfee", res);
        if (!service.empty()) {
            res = get<double>(service + ".maxfee", res);
            res = get<double>(service + xrdelimiter + cstr + ".maxfee", res);
        }
    }

    return res;
}

std::string XRouterSettings::help(XRouterCommand c, const std::string & service)
{
    const std::string cstr{XRouterCommand_ToString(c)};
    auto res = get<std::string>("Main.help", "");
    std::string pres;

    if (c == xrService && hasPlugin(service)) {
        auto ps = getPluginSettings(service);
        pres = ps->has("help") ? ps->help() : res;
    }

    if (pres.empty()) {
        res = get<std::string>(cstr + ".help", res);
        if (!service.empty()) {
            if (c == xrService)
                res = get<std::string>(cstr + xrdelimiter + service + ".help", res);
            else {
                res = get<std::string>(service + ".help", res);
                res = get<std::string>(service + xrdelimiter + cstr + ".help", res);
            }
        }
    } else
        res = pres;

    return res;
}

int XRouterSettings::commandTimeout(XRouterCommand c, const std::string & service, int def)
{
    const std::string cstr{XRouterCommand_ToString(c)};
    auto res = get<int>("Main.timeout", def);

    if (c == xrService) { // Handle plugin
        if (!service.empty())
            res = get<int>(cstr + xrdelimiter + service + ".timeout", res);
    } else {
        res = get<int>(cstr + ".timeout", res);
        if (!service.empty()) {
            res = get<int>(service + ".timeout", res);
            res = get<int>(service + xrdelimiter + cstr + ".timeout", res);
        }
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
            res = get<int>(cstr + xrdelimiter + service + ".consensus", res);
    } else {
        res = get<int>(cstr + ".consensus", res);
        if (!service.empty()) {
            res = get<int>(service + ".consensus", res);
            res = get<int>(service + xrdelimiter + cstr + ".consensus", res);
        }
    }

    return res;
}

double XRouterSettings::defaultFee() {
    return get<double>("Main.fee", 0);
}

double XRouterSettings::commandFee(XRouterCommand c, const std::string & service, double def)
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
    if (!service.empty()) {
        res = get<double>(service + ".fee", res);
        res = get<double>(service + xrdelimiter + std::string(XRouterCommand_ToString(c)) + ".fee", res);
    }
    return res;
}

int XRouterSettings::commandFetchLimit(XRouterCommand c, const std::string & service, int def)
{
    // Handle plugin
    if (c == xrService && hasPlugin(service)) {
        auto ps = getPluginSettings(service);
        if (ps->has("fetchlimit"))
            return maxFetchLimit(ps->fetchLimit());
        else
            return maxFetchLimit(get<int>("Main.fetchlimit", def));
    }

    auto res = get<int>("Main.fetchlimit", def);
    res = get<int>(std::string(XRouterCommand_ToString(c)) + ".fetchlimit", res);
    if (!service.empty()) {
        res = get<int>(service + ".fetchlimit", res);
        res = get<int>(service + xrdelimiter + std::string(XRouterCommand_ToString(c)) + ".fetchlimit", res);
    }
    return maxFetchLimit(res);
}    

int XRouterSettings::clientRequestLimit(XRouterCommand c, const std::string & service, int def) {
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
    if (!service.empty()) {
        res = get<int>(service + ".clientrequestlimit", res);
        res = get<int>(service + xrdelimiter + std::string(XRouterCommand_ToString(c)) + ".clientrequestlimit", res);
    }
    return res;
}

std::string XRouterSettings::paymentAddress(XRouterCommand c, const std::string & service) {
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

    auto addr = sn::ServiceNodePaymentAddress(node);
    // default payment address is snode vin address
    if (!boost::get<CNoDestination>(&addr))
        def = EncodeDestination(addr);
    auto res = get<std::string>(s_mainpaymentaddress, def);
    res = get<std::string>(std::string(XRouterCommand_ToString(c)) + "." + s_paymentaddress, res);
    if (!service.empty()) {
        res = get<std::string>(service + "." + s_paymentaddress, res);
        res = get<std::string>(service + xrdelimiter + std::string(XRouterCommand_ToString(c)) + "." + s_paymentaddress, res);
    }
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

    LOCK(mu);

    // First pass set top-level fees
    for (const auto & p : m_pt) {
        std::vector<std::string> parts;
        xrsplit(p.first, xrdelimiter, parts);
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
        xrsplit(p.first, xrdelimiter, parts);

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

    LOCK(mu);
    plugins[name] = settings;
    return true;
}

boost::filesystem::path XRouterSettings::pluginPath() const
{
    return GetDataDir(false) / "plugins";
}

void XRouterSettings::genPublic()
{
    LOCK(mu);
    std::string publictext;
    std::vector<std::string> lines;
    boost::split(lines, rawtext, boost::is_any_of("\n"));

    // Exclude commands with the private prefixes
    std::regex rprivateComment("^\\s*"+privateComment+".*$");
    std::smatch m;
    for (const std::string & line : lines) {
        if (line.find(privatePrefix) != std::string::npos || std::regex_match(line, m, rprivateComment))
            continue;
        publictext += line + "\n";
    }

    pubtext = publictext;
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
    
    return true;
}

bool XRouterPluginSettings::read(const std::string & config)
{
    if (!IniConfig::read(config))
        return false;
    if (!verify(config))
        return false;
    
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

void XRouterPluginSettings::genPublic()
{
    LOCK(mu);
    std::string publictext;
    std::vector<std::string> lines;
    boost::split(lines, rawtext, boost::is_any_of("\n"));

    // Exclude commands with the private prefixes
    std::regex rprivateComment("^\\s*"+privateComment+".*$");
    std::smatch m;
    for (const std::string & line : lines) {
        if (line.find(privatePrefix) != std::string::npos || std::regex_match(line, m, rprivateComment))
            continue;
        publictext += line + "\n";
    }

    pubtext = publictext;
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

int XRouterPluginSettings::fetchLimit() {
    int res = get<int>("fetchlimit", XROUTER_DEFAULT_FETCHLIMIT);
    return maxFetchLimit(res);
}

int XRouterPluginSettings::commandTimeout() {
    int res = get<int>("timeout", 30);
    return res;
}

std::string XRouterPluginSettings::paymentAddress() {
    auto res = get<std::string>("paymentaddress", "");
    return res;
}

bool XRouterPluginSettings::disabled() {
    auto res = get<bool>("disabled", false);
    return res;
}

bool XRouterPluginSettings::quoteArgs() {
    auto t = get<bool>("quoteargs", true);
    t = get<bool>(privatePrefix + "quoteargs", t);
    return t;
}

std::string XRouterPluginSettings::container() {
    auto t = get<std::string>("containername", "");
    t = get<std::string>(privatePrefix + "containername", t);
    return t;
}

std::string XRouterPluginSettings::command() {
    auto t = get<std::string>("command", "");
    t = get<std::string>(privatePrefix + "command", t);
    return t;
}

std::string XRouterPluginSettings::commandArgs() {
    auto t = get<std::string>("args", "");
    t = get<std::string>(privatePrefix + "args", t);
    return t;
}

bool XRouterPluginSettings::hasCustomResponse() {
    return has("response") || has(privatePrefix + "response");
}

std::string XRouterPluginSettings::customResponse() {
    auto t = get<std::string>("response", "");
    t = get<std::string>(privatePrefix + "response", t);
    return t;
}

std::string XRouterPluginSettings::help() {
    auto t = get<std::string>("help", "");
    return t;
}

void saveConf(const boost::filesystem::path & p, const std::string & str) {
    boost::filesystem::ofstream file;
    file.exceptions(std::ofstream::failbit | std::ofstream::badbit);
    file.open(p, std::ios_base::binary);
    file.write(str.c_str(), str.size());
}

bool createConf(const boost::filesystem::path & confDir, const bool & skipPlugins) {
    try {
        std::string eol = "\n";
#ifdef WIN32
        eol = "\r\n";
#endif
        auto p = confDir / "xrouter.conf";
        if (!boost::filesystem::exists(p)) {
            saveConf(p,
                     "[Main]"                                                                                            + eol +
                     "#! host is a mandatory field, this tells the XRouter network how to find your node."               + eol +
                     "#! DNS and ip addresses are acceptable values."                                                    + eol +
                     "#! host=mynode.example.com"                                                                        + eol +
                     "#! host=208.67.222.222"                                                                            + eol +
                     "host="                                                                                             + eol +
                     ""                                                                                                  + eol +
                     "#! port is the tcpip port on the host that accepts xrouter connections."                           + eol +
                     "#! port will default to the default blockchain port (e.g. 41412), examples:"                       + eol +
                     "#! port=41412"                                                                                     + eol +
                     "#! port=80"                                                                                        + eol +
                     "#! port=8080"                                                                                      + eol +
                     ""                                                                                                  + eol +
                     "#! tls signals to the xrouter network that your endpoint supports TLS/SSL connections."            + eol +
                     "#! The default is 0 (false)."                                                                      + eol +
                     "#! tls=1"                                                                                          + eol +
                     "tls=0"                                                                                             + eol +
                     ""                                                                                                  + eol +
                     "#! maxfee is the maximum fee (in BLOCK) you're willing to pay on a single xrouter call"            + eol +
                     "#! 0 means you only want free calls"                                                               + eol +
                     "maxfee=0"                                                                                          + eol +
                     ""                                                                                                  + eol +
                     "#! consensus is the minimum number of nodes you want your xrouter calls to query (1 or more)"      + eol +
                     "#! Paid calls will send a payment to each selected service node."                                  + eol +
                     "consensus=1"                                                                                       + eol +
                     ""                                                                                                  + eol +
                     "#! timeout is the maximum time in seconds you're willing to wait for an XRouter response"          + eol +
                     "timeout=30"                                                                                        + eol +
                     ""                                                                                                  + eol +
                     "#! Optionally set per-call config options:"                                                        + eol +
                     "#! [xrGetBlockCount]"                                                                              + eol +
                     "#! maxfee=0.01"                                                                                    + eol +
                     ""                                                                                                  + eol +
                     "#! [BLOCK::xrGetBlockCount]"                                                                       + eol +
                     "#! maxfee=0.01"                                                                                    + eol +
                     ""                                                                                                  + eol +
                     "#! [SYS::xrGetBlockCount]"                                                                         + eol +
                     "#! maxfee=0.01"                                                                                    + eol +
                     ""                                                                                                  + eol +
                     "#! It's possible to set config options for Custom XRouter services"                                + eol +
                     "#! [xrs::GetBestBlockHashBTC]"                                                                     + eol +
                     "#! maxfee=0.1"                                                                                     + eol +
                     ""                                                                                                  + eol +
                     "#! Plugin help documentation can be set here as well as in the plugin conf."                       + eol +
                     "#! [xrs::ExampleRPC]"                                                                              + eol +
                     "#! help=The plugin documentation here."                                                            + eol
            );
        }

        // Create the plugins directory if it doesn't exist
        auto plugins = confDir / "plugins";
        if (!skipPlugins && !boost::filesystem::exists(plugins)) {
            boost::filesystem::create_directory(plugins);
            auto samplerpc = plugins / "ExampleRPC.conf";
            saveConf(samplerpc,
                     "#! ExampleRPC is a sample rpc plugin. This entire plugin configuration is sent to the client."     + eol +
                     "#! Any lines beginning with #! will not be sent to the client."                                    + eol +
                     "#! Any config parameters beginning with private:: will not be sent to the client."                 + eol +
                     "#! The name of the plugin file ExampleRPC will be the service name broadcasted to the XRouter"     + eol +
                     "#! network. Acceptable plugin names may include the characters: a-z A-Z 0-9 -"                     + eol +
                     ""                                                                                                  + eol +
                     //                "#! Optional host identifying the location of the service. this tells the XRouter network how "     + eol +
                     //                "#! to find your node. The default is xrouter.conf [Main].host"                                     + eol +
                     //                "#! DNS and ip address are acceptable values."                                                      + eol +
                     //                "#! host=mynode.example.com"                                                                        + eol +
                     //                "#! host=208.67.222.222"                                                                            + eol +
                     //                ""                                                                                                  + eol +
                     //                "#! Optionally specify the port on the host that accepts xrouter connections for this plugin."      + eol +
                     //                "#! The default is xrouter.conf [Main].port"                                                        + eol +
                     //                "#! port=80"                                                                                        + eol +
                     //                ""                                                                                                  + eol +
                     "#! parameters that you need from the user, acceptable types: string,bool,int,double"               + eol +
                     "#! Example parameters=string,bool if you want to accept a string parameter and boolean"            + eol +
                     "#! parameter from an XRouter client."                                                              + eol +
                     "parameters="                                                                                       + eol +
                     ""                                                                                                  + eol +
                     "#! Set the fee in BLOCK to how much you want to charge for requests to this custom plugin."        + eol +
                     "#! Example fee=0.1 if you want to accept 0.1 BLOCK or 0 if you want the plugin to be free."        + eol +
                     "fee=0"                                                                                             + eol +
                     ""                                                                                                  + eol +
                     "#! Set the client request limit in milliseconds. -1 means unlimited. 50 means that a client"       + eol +
                     "#! can only request at most once per 50 milliseconds (i.e. 20 times per second). If client"        + eol +
                     "#! requests exceed this value they will be penalized and eventually banned by your node."          + eol +
                     "clientrequestlimit=-1"                                                                             + eol +
                     ""                                                                                                  + eol +
                     "#! Use the help parameter to provide your plugin's documentation."                                 + eol +
                     "help=The plugin documentation here."                                                               + eol +
                     ""                                                                                                  + eol +
                     "#! This is a sample configuration for the RPC plugin type for a syscoin plugin."                   + eol +
                     "#! private:: config entries will not be sent to XRouter clients. Below is a sample rpc"            + eol +
                     "#! plugin for syscoin running on 127.0.0.1:8370. This plugin accepts 0 parameters, as"             + eol +
                     "#! indicated by \"parameters=\" config above, and it will call the syscoin \"getblockcount\""      + eol +
                     "#! rpc command. The result will be forwarded onto the client."                                     + eol +
                     "private::type=rpc"                                                                                 + eol +
                     "private::rpcip=127.0.0.1"                                                                          + eol +
                     "private::rpcport=8370"                                                                             + eol +
                     "private::rpcuser=sysuser"                                                                          + eol +
                     "private::rpcpassword=sysuser_pass"                                                                 + eol +
                     "private::rpccommand=getblockcount"                                                                 + eol +
                     ""                                                                                                  + eol +
                     "#! JSON version and Content Type can be set on the rpc call:"                                      + eol +
                     "#!private::rpcjsonversion=2.0"                                                                     + eol +
                     "#!private::rpccontenttype=application/json"                                                        + eol +
                     ""                                                                                                  + eol +
                     "#! Disable this sample plugin"                                                                     + eol +
                     "disabled=1"                                                                                        + eol
            );
            auto sampledocker = plugins / "ExampleDocker.conf";
            saveConf(sampledocker,
                     "#! ExampleDocker is a sample docker plugin. This entire plugin configuration is sent to the client." + eol +
                     "#! Any lines beginning with #! will not be sent to the client."                                      + eol +
                     "#! Any config parameters beginning with private:: will not be sent to the client."                   + eol +
                     "#! The name of the plugin file ExampleDocker will be the service name broadcasted to the XRouter"    + eol +
                     "#! network. Acceptable plugin names may include the characters: a-z A-Z 0-9 -"                       + eol +
                     ""                                                                                                    + eol +
                     //                "#! Optional host identifying the location of the service. this tells the XRouter network how "       + eol +
                     //                "#! to find your node. The default is xrouter.conf [Main].host"                                       + eol +
                     //                "#! DNS and ip address are acceptable values."                                                        + eol +
                     //                "#! host=mynode.example.com"                                                                          + eol +
                     //                "#! host=208.67.222.222"                                                                              + eol +
                     //                ""                                                                                                    + eol +
                     //                "#! Optionally specify the port on the host that accepts xrouter connections for this plugin."        + eol +
                     //                "#! The default is xrouter.conf [Main].port"                                                          + eol +
                     //                "#! port=80"                                                                                          + eol +
                     //                ""                                                                                                    + eol +
                     "#! parameters that you need from the user, acceptable types: string,bool,int,double"                 + eol +
                     "#! Example parameters=string,bool if you want to accept a string parameter and boolean"              + eol +
                     "#! parameter from an XRouter client."                                                                + eol +
                     "parameters=string"                                                                                   + eol +
                     ""                                                                                                    + eol +
                     "#! Set the fee in BLOCK to how much you want to charge for requests to this custom plugin."          + eol +
                     "#! Example fee=0.1 if you want to accept 0.1 BLOCK or 0 if you want the plugin to be free."          + eol +
                     "fee=0"                                                                                               + eol +
                     ""                                                                                                    + eol +
                     "#! Set the client request limit in milliseconds. -1 means unlimited. 50 means that a client"         + eol +
                     "#! can only request at most once per 50 milliseconds (i.e. 20 times per second). If client"          + eol +
                     "#! requests exceed this value they will be penalized and eventually banned by your node."            + eol +
                     "clientrequestlimit=-1"                                                                               + eol +
                     ""                                                                                                    + eol +
                     "#! Use the help parameter to provide your plugin's documentation."                                   + eol +
                     "help=The plugin documentation here."                                                                 + eol +
                     ""                                                                                                    + eol +
                     "#! This is a sample configuration of a docker plugin running a syscoin container."                   + eol +
                     "#! private:: config entries will not be sent to XRouter clients. Below is a sample rpc"              + eol +
                     "#! plugin for syscoin running in docker container \"syscoin\". This plugin accepts 1 parameter"      + eol +
                     "#! indicated by \"parameters=\" config above, and it will call the syscoin \"getblock\" rpc"         + eol +
                     "#! command. The result will be forwarded onto the client. \"quoteargs\" puts \"$1\" around"          + eol +
                     "#! user supplied arguments. \"command\" executed within the docker container. \"args\" can"          + eol +
                     "#! include both user supplied arguments ($1, $2, $3 etc.) and explicit arguments. For example,"      + eol +
                     "#! you can mix both user supplied and custom arguments:  private::args=some_api_key $1 $2"           + eol +
                     "private::type=docker"                                                                                + eol +
                     "private::containername=syscoin"                                                                      + eol +
                     "private::quoteargs=1"                                                                                + eol +
                     "private::command=syscoin-cli getblock"                                                               + eol +
                     "private::args=$1"                                                                                    + eol +
                     ""                                                                                                    + eol +
                     "#! Disable this sample plugin"                                                                       + eol +
                     "disabled=1"                                                                                          + eol
            );
        }

        return true;

    } catch (...) {
        ERR() << "XRouter failed to create default xrouter.conf and plugins directory";
    }

    return false;
}

} // namespace xrouter
