// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetaddressedit.h"
#include "blocknethdiv.h"

#include <QMessageBox>
#include <QKeyEvent>
#include <QDoubleValidator>

BlocknetAddressEdit::BlocknetAddressEdit(WalletModel *w, bool e, QFrame *parent) : QFrame(parent), editMode(e), walletModel(w),
                                                                                 layout(new QVBoxLayout) {
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(layout);
    layout->setContentsMargins(46, 10, 50, 30);
    layout->setSpacing(0);

    addressLbl = new QLabel(tr("Address Book"));
    addressLbl->setObjectName("h4");

    QString titleString = QString("Add Address");
    if (editMode) titleString = QString("Edit Address");
    QLabel *titleLbl = new QLabel(titleString);
    titleLbl->setObjectName("h2");

    addressTi = new BlocknetLineEditWithTitle(tr("Address"), tr("Enter Address..."), 675);
    aliasTi = new BlocknetLineEditWithTitle(tr("Alias (optional)"), tr("Enter alias..."));

    auto *radioGrid = new QFrame;
    radioGrid->setObjectName("radioGrid");
    QGridLayout *radioLayout = new QGridLayout;
    radioLayout->setContentsMargins(QMargins());
    radioLayout->setVerticalSpacing(0);
    radioLayout->setColumnStretch(0, 0);
    radioLayout->setColumnStretch(1, 1);
    radioGrid->setLayout(radioLayout);

    myAddressBtn = new QRadioButton(tr("My Address"));
    otherUserBtn = new QRadioButton(tr("Other User"));
    otherUserBtn->setObjectName("otherUserBtn");

    radioLayout->addWidget(myAddressBtn, 0, 0);
    radioLayout->addWidget(otherUserBtn, 0, 1);

    auto *div1 = new BlocknetHDiv;

    auto *buttonGrid = new QFrame;
    QGridLayout *buttonLayout = new QGridLayout;
    buttonLayout->setContentsMargins(15, 0, 15, 0);
    buttonLayout->setColumnStretch(0, 0);
    buttonLayout->setColumnStretch(1, 2);
    buttonGrid->setLayout(buttonLayout);

    confirmBtn = new BlocknetFormBtn;
    QString buttonString = QString("Add Address");
    if (editMode) buttonString = QString("Apply");
    confirmBtn->setText(buttonString);
    cancelBtn = new BlocknetFormBtn;
    cancelBtn->setObjectName("cancel");
    cancelBtn->setText(tr("Cancel"));

    buttonLayout->addWidget(cancelBtn, 0, 0, Qt::AlignLeft);
    buttonLayout->addWidget(confirmBtn, 0, 1, Qt::AlignLeft);

    layout->addWidget(addressLbl);
    layout->addSpacing(45);
    layout->addWidget(titleLbl);
    layout->addSpacing(20);
    layout->addWidget(addressTi);
    layout->addWidget(aliasTi);
    layout->addSpacing(20);
    layout->addWidget(radioGrid);
    layout->addSpacing(30);
    layout->addWidget(div1);
    layout->addSpacing(60);
    layout->addWidget(buttonGrid);
    layout->addStretch(1);
}

void BlocknetAddressEdit::clear() {
    //proposalTi->clear();
}