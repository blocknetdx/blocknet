// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETTOOLS_H
#define BLOCKNETTOOLS_H

#include "blocknettabbar.h"

#include "walletmodel.h"

#include <QFrame>
#include <QVBoxLayout>
#include <QSet>

class BlocknetToolsPage : public QFrame {
    Q_OBJECT
public:
    explicit BlocknetToolsPage(int id, QFrame *parent = nullptr);

protected:
    WalletModel *walletModel;
    int pageID{0};
};

class BlocknetDebugConsole;
class BlocknetPeersList;
class BlocknetBIP38Tool;
class BlocknetWalletRepair;

class BlocknetTools : public QFrame {
    Q_OBJECT
public:
    explicit BlocknetTools(QFrame *parent = nullptr);
    void setWalletModel(WalletModel *w);

protected:

private slots:
    void tabChanged(int tab);

private:
    WalletModel *walletModel;
    QVector<BlocknetToolsPage*> pages;

    QVBoxLayout *layout;
    QLabel *titleLbl;
    BlocknetDebugConsole *debugConsole;
    BlocknetPeersList *networkMonitor;
    BlocknetPeersList *peersList;
    BlocknetBIP38Tool *bip38Tool;
    BlocknetWalletRepair *walletRepair;
    BlocknetPeersList *multisend;
    BlocknetTabBar *tabBar;
    BlocknetToolsPage *screen = nullptr;
};

#endif // BLOCKNETTOOLS_H
