// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETTOOLSPAGE_H
#define BLOCKNET_QT_BLOCKNETTOOLSPAGE_H

#include <qt/blocknettabbar.h>

#include <qt/clientmodel.h>
#include <qt/walletmodel.h>

#include <QFrame>

class BlocknetToolsPage : public QFrame {
    Q_OBJECT
public:
    explicit BlocknetToolsPage(int id, QFrame *parent = nullptr) {}

protected:
    WalletModel *walletModel = nullptr;
    int pageID{0};
};

#endif // BLOCKNET_QT_BLOCKNETTOOLSPAGE_H
