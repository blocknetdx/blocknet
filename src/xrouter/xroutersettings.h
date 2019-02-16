//******************************************************************************
//******************************************************************************
#ifndef XROUTERSETTINGS_H
#define XROUTERSETTINGS_H

#include <vector>
#include <string>
#include "xrouterpacket.h"
#include "xrouterdef.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/container/map.hpp>

#define TRY(_STMNT_) try { (_STMNT_); } catch(std::exception & e) { LOG() << e.what(); }

namespace xrouter
{

class XRouterPluginSettings;
    
class IniConfig
{
public:
    IniConfig() {}
    bool read(const char * fileName = 0);
    bool read(std::string config);
    bool write(const char * fileName = 0);
    
    std::string rawText() const { return rawtext; }
    
    template <class _T>
    _T get(const std::string & param, _T def = _T())
    {
        return get<_T>(param.c_str(), def);
    }

    template <class _T>
    _T get(const char * param, _T def = _T())
    {
        _T tmp = def;
        try
        {
            tmp = m_pt.get<_T>(param);
            return tmp;
        }
        catch (std::exception & e)
        {
            //LOG() << e.what();
        }

        return tmp;
    }
    
    template <class _T>
    _T set(const std::string & param, _T def = _T())
    {
        return set<_T>(param.c_str(), def);
    }
    
    template <class _T>
    bool set(const char * param, const _T & val)
    {
        try
        {
            m_pt.put<_T>(param, val);
            write();
        }
        catch (std::exception & e)
        {
            //LOG() << e.what();
            return false;
        }
        return true;
    }
    
protected:
    std::string m_fileName;
    boost::property_tree::ptree m_pt;
    std::string rawtext;
};

class XRouterPluginSettings : public IniConfig
{
public:
    XRouterPluginSettings() {}
    std::string getParam(std::string param, std::string def="");
    double getFee();
    int getMinParamCount();
    int getMaxParamCount();
    std::string rawText() { return publictext; }
    std::string fullText() { return rawtext; }
    
    bool read(const char * fileName = 0);
    bool read(std::string config);
    
    bool verify(std::string name="");
private:
    void formPublicText();
    std::string publictext;
};

//******************************************************************************
class XRouterSettings : public IniConfig
{
public:
    XRouterSettings() {}

    void loadPlugins();
    bool loadPlugin(std::string name);
    std::string pluginPath() const;
    void addPlugin(std::string name, XRouterPluginSettings s) { plugins[name] = s; pluginList.push_back(name); }

    bool walletEnabled(std::string currency);
    bool isAvailableCommand(XRouterCommand c, std::string currency="", bool def=true);
    double getCommandFee(XRouterCommand c, std::string currency="", double def=0.0);
    double getCommandTimeout(XRouterCommand c, std::string currency="", double def=XROUTER_DEFAULT_TIMEOUT);
    int getCommandBlockLimit(XRouterCommand c, std::string currency="", double def=XROUTER_DEFAULT_BLOCK_LIMIT);
    double getMaxFee(XRouterCommand c, std::string currency="", double def=0.0);
    int clientRequestLimit(XRouterCommand c, std::string currency="", int def=-1); // -1 is no limit
    bool hasPlugin(std::string name);
    std::vector<std::string>& getPlugins() { return pluginList; }
    XRouterPluginSettings& getPluginSettings(std::string name) { return plugins[name]; }
    
private:
    boost::container::map<std::string, XRouterPluginSettings > plugins;
    std::vector<std::string> pluginList;
};

} // namespace

#endif // SETTINGS_H
