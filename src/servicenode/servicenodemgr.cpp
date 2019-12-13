// Copyright (c) 2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <servicenode/servicenodemgr.h>

namespace sn {

CTxDestination ServiceNodePaymentAddress(const std::string & snode) {
    // default payment address is snode vin address
    auto s = sn::ServiceNodeMgr::instance().getSn(snode);
    if (!s.isNull())
        return CTxDestination(s.getPaymentAddress());
    return CNoDestination{};
}

}
