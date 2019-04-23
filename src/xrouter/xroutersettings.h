//******************************************************************************
//******************************************************************************
#ifndef XROUTERSETTINGS_H
#define XROUTERSETTINGS_H

#include "xrouterpacket.h"
#include "xrouterdef.h"

#include "sync.h"

#include <vector>
#include <string>

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/container/map.hpp>
#include <boost/algorithm/string.hpp>

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
        WaitableLock l(mu);
        return rawtext;
    }
    virtual std::string publicText() const {
        WaitableLock l(mu);
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
            WaitableLock l(mu);
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
                WaitableLock l(mu);
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
    mutable CWaitableCriticalSection mu;
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
        WaitableLock l(mu);
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
    explicit XRouterSettings(const bool & ismine = true);

    bool init(const boost::filesystem::path & configPath);
    bool init(const std::string & config);

    void defaultPaymentAddress(const std::string & paymentAddress);

    void assignNode(const std::string & node) {
        WaitableLock l(mu);
        this->node = node;
    }
    std::string getNode() {
        WaitableLock l(mu);
        return this->node;
    }

    void loadWallets();
    std::vector<std::string> getWallets() {
        WaitableLock l(mu);
        return {wallets.begin(), wallets.end()};
    }
    bool hasWallet(const std::string & currency);

    void loadPlugins();
    std::vector<std::string> getPlugins() {
        WaitableLock l(mu);
        return {pluginList.begin(),pluginList.end()};
    }
    bool hasPlugin(const std::string & name);


    void addPlugin(const std::string &name, XRouterPluginSettingsPtr s) {
        WaitableLock l(mu);
        plugins[name] = s; pluginList.insert(name);
    }

    XRouterPluginSettingsPtr getPluginSettings(const std::string & name) {
        WaitableLock l(mu);
        return plugins[name];
    }

    bool isAvailableCommand(XRouterCommand c, const std::string & service);
    double commandFee(XRouterCommand c, const std::string & service, double def=0.0);
    int commandTimeout(XRouterCommand c, const std::string & service, int def=XROUTER_DEFAULT_TIMEOUT);
    int commandFetchLimit(XRouterCommand c, const std::string & service, int def=XROUTER_DEFAULT_FETCHLIMIT);
    double maxFee(XRouterCommand c, std::string currency="", double def=0.0);
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
    std::string node;
    bool ismine{true}; // indicating if the config is our own
};

} // namespace

#endif // SETTINGS_H
