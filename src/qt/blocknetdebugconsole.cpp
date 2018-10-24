// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetdebugconsole.h"

BlocknetDebugConsole::BlocknetDebugConsole(QWidget *popup, int id, QFrame *parent) : BlocknetToolsPage(id, parent), popupWidget(popup), layout(new QVBoxLayout) {
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(layout);
    layout->setContentsMargins(0, 10, 0, 10);

    titleLbl = new QLabel(tr("Debug Console"));
    titleLbl->setObjectName("h2");

    layout->addWidget(titleLbl, 0, Qt::AlignTop | Qt::AlignLeft);
    layout->addSpacing(5);
}

void BlocknetDebugConsole::setWalletModel(WalletModel *w) {
    if (!walletModel)
        return;

    walletModel = w;
}
