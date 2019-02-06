// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetbip38tool.h"

#include "blocknethdiv.h"
#include "blocknetkeydialog.h"

#include <QDoubleValidator>
#include <QIntValidator>
#include <QHBoxLayout>

BlocknetBIP38Tool::BlocknetBIP38Tool(QWidget *popup, int id, QFrame *parent) : BlocknetToolsPage(id, parent), popupWidget(popup), layout(new QVBoxLayout) {
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(layout);
    layout->setContentsMargins(QMargins());
    layout->setSpacing(0);

    content = new QFrame;
    content->setContentsMargins(QMargins());
    content->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum);
    content->setObjectName("contentFrame");
    contentLayout = new QVBoxLayout;
    contentLayout->setContentsMargins(QMargins());
    content->setLayout(contentLayout);
    
    scrollArea = new QScrollArea;
    scrollArea->setContentsMargins(QMargins());
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setWidget(content);

    titleLbl = new QLabel(tr("BIP38 Encrypt"));
    titleLbl->setObjectName("h2");

    addressTi = new BlocknetLineEditWithTitle(tr("Address"), tr("Enter Blocknet address..."), 100);
    addressTi->setObjectName("proposal");
    addressTi->setMaximumWidth(500);
    addressTi->lineEdit->setFocusPolicy(Qt::FocusPolicy::StrongFocus);
    addressTi->lineEdit->setValidator(new QRegExpValidator(QRegExp("[a-zA-Z0-9-_]+"), this));
    addressTi->lineEdit->setMaxLength(20);

    addressBtn = new BlocknetAddressBtn;

    auto *addressBtnFrame = new QFrame;
    auto *addressBtnLayout = new QVBoxLayout;
    addressBtnFrame->setContentsMargins(QMargins());
    addressBtnLayout->setContentsMargins(0, 0, 0, 0);
    addressBtnFrame->setLayout(addressBtnLayout);
    addressBtnLayout->addWidget(addressBtn, 0, Qt::AlignBottom);

    auto *addressFrame = new QFrame;
    auto *addressLayout = new QHBoxLayout;
    addressFrame->setContentsMargins(QMargins());
    addressLayout->setContentsMargins(0, 0, 0, 0);
    addressLayout->setSpacing(10);
    addressFrame->setLayout(addressLayout);
    addressLayout->addWidget(addressTi, 0, Qt::AlignLeft);
    addressLayout->addWidget(addressBtnFrame, 0, Qt::AlignJustify);
    addressLayout->addStretch(1);

    addressPasswordTi = new BlocknetLineEditWithTitle(tr("Password"), tr("Enter password..."), 100);
    addressPasswordTi->setObjectName("proposal");
    addressPasswordTi->setMaximumWidth(500);
    addressPasswordTi->lineEdit->setFocusPolicy(Qt::FocusPolicy::StrongFocus);
    addressPasswordTi->lineEdit->setMaxLength(20);

    generateBtn = new BlocknetFormBtn;
    generateBtn->setText(tr("Generate Encryption Key"));

    auto *div1 = new BlocknetHDiv;

    keyTi = new BlocknetLineEditWithTitle(tr("Encrypted Key"), tr("Enter encrypted key..."), 100);
    keyTi->setObjectName("proposal");
    keyTi->setMaximumWidth(500);
    keyTi->lineEdit->setFocusPolicy(Qt::FocusPolicy::StrongFocus);
    keyTi->lineEdit->setValidator(new QRegExpValidator(QRegExp("[a-zA-Z0-9-_]+"), this));
    keyTi->lineEdit->setMaxLength(20);

    keyPasswordTi = new BlocknetLineEditWithTitle(tr("Password"), tr("Enter password..."), 100);
    keyPasswordTi->setObjectName("proposal");
    keyPasswordTi->setMaximumWidth(500);
    keyPasswordTi->lineEdit->setFocusPolicy(Qt::FocusPolicy::StrongFocus);
    keyPasswordTi->lineEdit->setMaxLength(20);

    decryptBtn = new BlocknetFormBtn;
    decryptBtn->setText(tr("Decrypt Key"));

    contentLayout->addWidget(titleLbl, 0, Qt::AlignTop | Qt::AlignLeft);
    contentLayout->addSpacing(10);
    contentLayout->addWidget(addressFrame);
    contentLayout->addSpacing(5);
    contentLayout->addWidget(addressPasswordTi);
    //contentLayout->addSpacing(5);
    contentLayout->addWidget(generateBtn);
    contentLayout->addSpacing(20);
    contentLayout->addWidget(div1);
    contentLayout->addSpacing(10);
    contentLayout->addWidget(keyTi);
    contentLayout->addSpacing(5);
    contentLayout->addWidget(keyPasswordTi);
    contentLayout->addSpacing(5);
    contentLayout->addWidget(decryptBtn);
    contentLayout->addStretch(1);

    layout->addWidget(scrollArea);

    connect(generateBtn, &BlocknetFormBtn::clicked, this, &BlocknetBIP38Tool::onGenerateEncryptedKey);
    connect(decryptBtn, &BlocknetFormBtn::clicked, this, &BlocknetBIP38Tool::onDecryptKey);
}

void BlocknetBIP38Tool::setWalletModel(WalletModel *w) {
    if (!walletModel)
        return;

    walletModel = w;
}

void BlocknetBIP38Tool::onGenerateEncryptedKey() {
    BlocknetKeyDialog dlg(tr("MlgLuh-fsFDJ-Fjhabcsc-dIOJbda-nbcsc-dIOJbda-ncacjttt"), tr("Address: POKLuhfsFDJFjhabcscdIOJbdancacnksd"), tr("Add to Address Book"), tr("Your Encrypted Key"), this);
    dlg.setFixedSize(500, 280);
    connect(&dlg, &QDialog::accepted, this, [this, &dlg]() {
        // ...
    });
    dlg.exec();
} 

void BlocknetBIP38Tool::onDecryptKey() {
    BlocknetKeyDialog dlg(tr("MlgLuh-fsFDJ-Fjhabcsc-dIOJbda-nbcsc-dIOJbda-ncacjttt"), tr("Address: POKLuhfsFDJFjhabcscdIOJbdancacnksd"), tr("Add to Address Book"), tr("Your Decrypted Key"), this);
    dlg.setFixedSize(500, 280);
    connect(&dlg, &QDialog::accepted, this, [this, &dlg]() {
        // ...
    });
    dlg.exec();
} 
