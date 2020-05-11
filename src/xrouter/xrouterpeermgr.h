// Copyright (c) 2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_XROUTER_XROUTERPEERMGR_H
#define BLOCKNET_XROUTER_XROUTERPEERMGR_H

#include <banman.h>
#include <net.h>
#include <net_processing.h>
#include <pubkey.h>
#include <servicenode/servicenodemgr.h>
#include <xrouter/xroutersnodeconfig.h>

namespace xrouter {

class XRouterPeerMgr final : public NetEventsInterface {
public: // NetEventsInterface
    XRouterPeerMgr(CConnman* connman, CScheduler & scheduler, sn::ServiceNodeMgr & serviceNodeMgr);
    void InitializeNode(CNode* pnode) override;
    void FinalizeNode(NodeId nodeid, bool& fUpdateConnectionTime) override;
    bool ProcessMessages(CNode* pfrom, std::atomic<bool>& interrupt) override;
    bool SendMessages(CNode* pto) override EXCLUSIVE_LOCKS_REQUIRED(pto->cs_sendProcessing);
public:
    void callHandler(const sn::ServiceNode & snode);
    void onSnodePing(std::function<void (const sn::ServiceNode & snode)> phandler);
    sn::ServiceNodeMgr & snodeMgr();
private:
    CConnman *connman;
    CScheduler & scheduler;
    sn::ServiceNodeMgr & smgr;
    std::map<CPubKey, sn::ServiceNode> snodes;
    std::function<void (const sn::ServiceNode & snode)> handler;
};

}

#endif //BLOCKNET_XROUTER_XROUTERPEERMGR_H
