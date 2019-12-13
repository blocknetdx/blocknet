// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETTOOLS_H
#define BLOCKNET_QT_BLOCKNETTOOLS_H

#include <qt/blocknettabbar.h>
#include <qt/blocknettoolspage.h>

#include <qt/clientmodel.h>
#include <qt/walletmodel.h>

#include <QFocusEvent>
#include <QFrame>
#include <QSet>
#include <QVBoxLayout>

class BlocknetDebugConsole;
class BlocknetPeersList;
class BlocknetBIP38Tool;
class BlocknetWalletRepair;

class BlocknetTools : public QFrame {
    Q_OBJECT
public:
    explicit BlocknetTools(interfaces::Node & node, const PlatformStyle *platformStyle, QFrame *parent = nullptr);
    void setModels(ClientModel *c, WalletModel *w);

Q_SIGNALS:
    /** Get restart command-line parameters and handle restart */
    void handleRestart(QStringList args);

protected:
    void focusInEvent(QFocusEvent *evt);

private Q_SLOTS:
    void tabChanged(int tab);

private:
    WalletModel *walletModel;
    ClientModel *clientModel;
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

#endif // BLOCKNET_QT_BLOCKNETTOOLS_H
