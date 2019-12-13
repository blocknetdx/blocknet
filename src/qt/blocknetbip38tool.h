// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETBIP38TOOL_H
#define BLOCKNET_QT_BLOCKNETBIP38TOOL_H

#include <qt/blocknetaddressbtn.h>
#include <qt/blocknetformbtn.h>
#include <qt/blocknetlineeditwithtitle.h>
#include <qt/blocknettoolspage.h>

#include <qt/walletmodel.h>

#include <QFrame>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

class BlocknetBIP38Tool : public BlocknetToolsPage {
    Q_OBJECT

public:
    explicit BlocknetBIP38Tool(QWidget *popup, int id, QFrame *parent = nullptr);
    void setWalletModel(WalletModel *w);

private Q_SLOTS:
    void onGenerateEncryptedKey();
    void onDecryptKey();

private:
    QScrollArea *scrollArea;
    QFrame *content;
    QVBoxLayout *contentLayout;
    QVBoxLayout *layout;
    QLabel *titleLbl;
    QWidget *popupWidget;
    BlocknetLineEditWithTitle *addressTi;
    BlocknetAddressBtn *addressBtn;
    BlocknetLineEditWithTitle *addressPasswordTi;
    BlocknetFormBtn *generateBtn;
    BlocknetLineEditWithTitle *keyTi;
    BlocknetLineEditWithTitle *keyPasswordTi;
    BlocknetFormBtn *decryptBtn;
};

#endif // BLOCKNET_QT_BLOCKNETBIP38TOOL_H
