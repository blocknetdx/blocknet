// Copyright (c) 2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <xrouter/xroutersnodeconfig.h>

namespace xrouter {

XRouterSnodeConfig::XRouterSnodeConfig(const sn::ServiceNode & snode) {
    if (snode.isNull())
        return; // if bad snode, skip

    const auto & rawconfig = snode.getConfig("xrouter");
    UniValue uv;
    if (!uv.read(rawconfig))
        return;

    xrsettings = MakeUnique<XRouterSettings>(snode.getSnodePubKey(), false); // not our config
    try {
        const auto uvconf = find_value(uv, "config");
        if (uvconf.isNull() || !uvconf.isStr())
            return;
        if (!xrsettings->init(uvconf.get_str()))
            return;
    } catch (...) {
        return;
    }

    const auto & plugins = find_value(uv, "plugins");
    if (!plugins.isNull()) {
        std::map<std::string, UniValue> kv; plugins.getObjMap(kv);
        for (const auto & item : kv) {
            const auto & plugin = item.first;
            const auto & config = item.second.get_str();
            try {
                auto psettings = std::make_shared<XRouterPluginSettings>(false); // not our config
                psettings->read(config);
                // Exclude open tier paid services
                if (!(snode.getTier() == sn::ServiceNode::OPEN && psettings->fee() > std::numeric_limits<double>::epsilon()))
                    xrsettings->addPlugin(plugin, psettings);
            } catch (...) { }
        }
    }

    valid = true;
}

const std::unique_ptr<XRouterSettings>& XRouterSnodeConfig::settings() const {
    return xrsettings;
}

bool XRouterSnodeConfig::isValid() const {
    return valid;
}

}
