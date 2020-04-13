// Copyright (c) 2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_TEST_XROUTER_TESTS_H
#define BLOCKNET_TEST_XROUTER_TESTS_H

#include <chainparamsbase.h>
#include <xrouter/xrouterclient.h>
#include <xrouter/xrouterutils.h>
#include <string>
#include <utility>
#include <vector>

struct XRouterTestClient {
    explicit XRouterTestClient();
    ~XRouterTestClient();
    CConnman::Options connOptions;
    std::unique_ptr<xrouter::XRouterClient> client = nullptr;
};
struct XRouterTestClientMainnet : public XRouterTestClient {
    explicit XRouterTestClientMainnet();
};
struct XRouterTestClientTestnet : public XRouterTestClient {
    explicit XRouterTestClientTestnet();
};

#endif //BLOCKNET_TEST_XROUTER_TESTS_H
