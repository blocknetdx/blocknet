// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETBIP38TOOL_H
#define BLOCKNETBIP38TOOL_H

#include "blocknettools.h"
#include "blocknetlineeditwithtitle.h"
#include "blocknetformbtn.h"
#include "blocknetaddressbtn.h"

#include <QFrame>
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollArea>

class BlocknetBIP38Tool : public BlocknetToolsPage {
    Q_OBJECT

public:
    explicit BlocknetBIP38Tool(QWidget *popup, int id, QFrame *parent = nullptr);
    void setWalletModel(WalletModel *w);

private slots:
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

#endif // BLOCKNETBIP38TOOL_H
