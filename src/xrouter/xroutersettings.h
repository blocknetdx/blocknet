// Copyright (c) 2018-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_XROUTER_XROUTERSETTINGS_H
#define BLOCKNET_XROUTER_XROUTERSETTINGS_H

#include <xrouter/xrouterpacket.h>
#include <xrouter/xrouterdef.h>

#include <netaddress.h>
#include <sync.h>

#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#define TRY(_STMNT_) try { (_STMNT_); } catch(std::exception & e) { LOG() << e.what(); }

namespace xrouter
{
    
class IniConfig
{
public:
    IniConfig() = default;

    virtual bool read(const boost::filesystem::path & fileName);
    virtual bool read(const std::string & config);
    virtual bool write(const char * fileName = nullptr);
    
    virtual std::string rawText() const {
        LOCK(mu);
        return rawtext;
    }
    virtual std::string publicText() const {
        LOCK(mu);
        return pubtext;
    }
    
    template <typename _T>
    _T get(const std::string & param, _T def = _T())
    {
        return get<_T>(param.c_str(), def);
    }

    template <typename _T>
    _T get(const char * param, _T def = _T())
    {
        _T tmp = def;
        try
        {
            LOCK(mu);
            tmp = m_pt.get<_T>(param);
            return tmp;
        }
        catch (std::exception & e) { }

        return tmp;
    }
    
    template <typename _T>
    bool set(const std::string & param, _T def = _T())
    {
        return set<_T>(param.c_str(), def);
    }
    
    template <typename _T>
    bool set(const char * param, const _T & val)
    {
        try
        {
            {
                LOCK(mu);
                m_pt.put<_T>(param, val);

                std::ostringstream oss;
                boost::property_tree::ini_parser::write_ini(oss, m_pt);
                rawtext = oss.str();
            }
            genPublic();
        }
        catch (std::exception & e) {
            return false;
        }

        return true;
    }

protected:
    virtual void genPublic() = 0;

protected:
    std::string m_fileName;
    boost::property_tree::ptree m_pt;
    std::string rawtext;
    std::string pubtext;
    mutable Mutex mu;
};

class XRouterPluginSettings;
typedef std::shared_ptr<XRouterPluginSettings> XRouterPluginSettingsPtr;
class XRouterPluginSettings : public IniConfig
{
public:
    explicit XRouterPluginSettings(const bool & ismine = true) : ismine(ismine) { }

    std::string stringParam(const std::string & param, const std::string def = "");
    std::string type();
    double fee();
    std::vector<std::string> parameters();
    int clientRequestLimit();
    int fetchLimit();
    std::string help();
    int commandTimeout();
    std::string paymentAddress();
    bool disabled();
    bool quoteArgs();
    std::string container();
    std::string command();
    std::string commandArgs();
    bool hasCustomResponse();
    std::string customResponse();

    bool read(const boost::filesystem::path & fileName) override;
    bool read(const std::string & config) override;

    bool verify(const std::string & name);
    bool has(const std::string & key) {
        LOCK(mu);
        return m_pt.count(key) > 0;
    }

protected:
    void genPublic() override;

private:
    bool ismine{true};
};

//******************************************************************************
class XRouterSettings : public IniConfig
{
public:
    explicit XRouterSettings(const CPubKey & pubkey, const bool & ismine = true);

    bool init(const boost::filesystem::path & configPath, bool snode = false);
    bool init(const std::string & config, bool snode = true); // assume string configs come from snodes
    void defaultPaymentAddress(const std::string & paymentAddress);

    const CService & getAddr() const {
        LOCK(mu);
        return addr;
    }
    std::string getNode() {
        LOCK(mu);
        return node;
    }

    void loadWallets();
    std::vector<std::string> getWallets() {
        LOCK(mu);
        return {wallets.begin(), wallets.end()};
    }
    bool hasWallet(const std::string & currency);

    void loadPlugins();
    std::vector<std::string> getPlugins() {
        LOCK(mu);
        return {pluginList.begin(),pluginList.end()};
    }
    bool hasPlugin(const std::string & name);

    void addPlugin(const std::string &name, XRouterPluginSettingsPtr s) {
        LOCK(mu);
        plugins[name] = s; pluginList.insert(name);
    }

    XRouterPluginSettingsPtr getPluginSettings(const std::string & name) {
        LOCK(mu);
        return plugins[name];
    }

    CPubKey getSnodePubKey() {
        LOCK(mu);
        return snodePubKey;
    }

    bool isAvailableCommand(XRouterCommand c, const std::string & service);
    std::string host(XRouterCommand c, const std::string & service="");
    int port(XRouterCommand c, const std::string & service="");
    bool tls(XRouterCommand c, const std::string & service="");
    double commandFee(XRouterCommand c, const std::string & service, double def=0.0);
    std::string help(XRouterCommand c, const std::string & service);
    int commandTimeout(XRouterCommand c, const std::string & service, int def=XROUTER_DEFAULT_TIMEOUT);
    int commandFetchLimit(XRouterCommand c, const std::string & service, int def=XROUTER_DEFAULT_FETCHLIMIT);
    double maxFee(XRouterCommand c, const std::string& currency="", double def=0.0);
    int clientRequestLimit(XRouterCommand c, const std::string & service, int def=-1); // -1 is no limit
    int confirmations(XRouterCommand c, std::string currency="", int def=XROUTER_DEFAULT_CONFIRMATIONS); // 1 confirmation default
    std::string paymentAddress(XRouterCommand c, const std::string & service="");
    int configSyncTimeout();

    double defaultFee();
    std::map<std::string, double> feeSchedule();

protected:
    void genPublic() override;

private:
    boost::filesystem::path pluginPath() const;
    bool loadPlugin(const std::string & name);

private:
    std::map<std::string, XRouterPluginSettingsPtr> plugins;
    std::set<std::string> pluginList;
    std::set<std::string> wallets;
    CService addr;
    std::string node;
    bool ismine{true}; // indicating if the config is our own
    CPubKey snodePubKey;
};

void saveConf(const boost::filesystem::path& p, const std::string& str);
bool createConf(const boost::filesystem::path & confDir, const bool & skipPlugins);

} // namespace

#endif // BLOCKNET_XROUTER_XROUTERSETTINGS_H
