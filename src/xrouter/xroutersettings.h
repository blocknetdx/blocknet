//******************************************************************************
//******************************************************************************
#ifndef XROUTERSETTINGS_H
#define XROUTERSETTINGS_H

#include <vector>
#include <string>
#include "xrouterpacket.h"

#include <boost/property_tree/ptree.hpp>

#define TRY(_STMNT_) try { (_STMNT_); } catch(std::exception & e) { LOG() << e.what(); }

#define XROUTER_DEFAULT_TIMEOUT 2

namespace xrouter
{

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
    
//******************************************************************************
class XRouterSettings : public IniConfig
{
public:
    XRouterSettings() {}

    std::string logPath() const;
    std::string rawText() const { return rawtext; }

    bool walletEnabled(std::string currency);
    bool isAvailableCommand(XRouterCommand c, std::string currency="", bool def=true);
    double getCommandFee(XRouterCommand c, std::string currency="", double def=0.0);
    double getCommandTimeout(XRouterCommand c, std::string currency="", double def=XROUTER_DEFAULT_TIMEOUT);
    bool hasService(std::string name);
    std::string getServiceParam(std::string name, std::string param, std::string def="");
    double getServiceFee(std::string name);
    int getServiceParamCount(std::string name);
};

class XRouterPluginSettings : public IniConfig
{
public:
    XRouterPluginSettings() {}
};


} // namespace

#endif // SETTINGS_H
