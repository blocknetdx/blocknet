//******************************************************************************
//******************************************************************************
#ifndef XROUTERSETTINGS_H
#define XROUTERSETTINGS_H

#include <vector>
#include <string>
#include "xrouterpacket.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/container/map.hpp>

#define TRY(_STMNT_) try { (_STMNT_); } catch(std::exception & e) { LOG() << e.what(); }

#define XROUTER_DEFAULT_TIMEOUT 2

namespace xrouter
{

class XRouterPluginSettings;
    
class IniConfig
{
public:
    IniConfig() {}
    bool read(const char * fileName = 0);
    bool read(std::string config);
    
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
    int getParamCount();
};

//******************************************************************************
class XRouterSettings : public IniConfig
{
public:
    XRouterSettings() {}

    void loadPlugins();
    void loadPlugin(std::string name);
    std::string pluginPath() const;
    std::string rawText() const { return rawtext; }

    bool walletEnabled(std::string currency);
    bool isAvailableCommand(XRouterCommand c, std::string currency="", bool def=true);
    double getCommandFee(XRouterCommand c, std::string currency="", double def=0.0);
    double getCommandTimeout(XRouterCommand c, std::string currency="", double def=XROUTER_DEFAULT_TIMEOUT);
    bool hasPlugin(std::string name);
    XRouterPluginSettings& getPluginSettings(std::string name) { return plugins[name]; }
    
private:
    boost::container::map<std::string, XRouterPluginSettings > plugins;
};

} // namespace

#endif // SETTINGS_H
