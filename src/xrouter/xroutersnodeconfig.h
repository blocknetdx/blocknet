// Copyright (c) 2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_XROUTER_XROUTERSNODECONFIG_H
#define BLOCKNET_XROUTER_XROUTERSNODECONFIG_H

#include <servicenode/servicenode.h>
#include <util/memory.h>
#include <xrouter/xroutersettings.h>

namespace xrouter {

class XRouterSnodeConfig {
public:
    explicit XRouterSnodeConfig(const sn::ServiceNode & snode);
    const std::unique_ptr<XRouterSettings>& settings() const;
    bool isValid() const;

private:
    std::unique_ptr<XRouterSettings> xrsettings;
    bool valid{false};
};

typedef std::unique_ptr<XRouterSnodeConfig> XRouterSnodeConfigPtr;

}

#endif //BLOCKNET_XROUTER_XROUTERSNODECONFIG_H
