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
    //addressTi->setObjectName("proposal");

    aliasTi = new BlocknetLineEditWithTitle(tr("Alias (optional)"), tr("Enter alias..."));
    //BlocknetLineEditWithTitle->setObjectName("url");

    /*auto *compactGrid = new QFrame;
    QGridLayout *gridLayout = new QGridLayout;
    gridLayout->setContentsMargins(QMargins());
    gridLayout->setVerticalSpacing(0);
    compactGrid->setLayout(gridLayout);*/

    /*gridLayout->addWidget(paymentCountTi, 0, 0);
    gridLayout->addWidget(superBlockTi, 0, 1);
    gridLayout->addWidget(amountTi, 1, 0);*/

    auto *div1 = new BlocknetHDiv;

    /*auto *buttonGrid = new QFrame;
    QGridLayout *buttonLayout = new QGridLayout;
    buttonLayout->setContentsMargins(15, 0, 15, 0);
    buttonLayout->setColumnStretch(0, 0);
    buttonLayout->setColumnStretch(1, 2);
    buttonGrid->setLayout(buttonLayout);*/

    /*continueBtn = new BlocknetFormBtn;
    continueBtn->setText(tr("Continue"));
    cancelBtn = new BlocknetFormBtn;
    cancelBtn->setObjectName("cancel");
    cancelBtn->setText(tr("Cancel"));

    buttonLayout->addWidget(cancelBtn, 0, 0, Qt::AlignLeft);
    buttonLayout->addWidget(continueBtn, 0, 1, Qt::AlignLeft);*/

    layout->addWidget(addressLbl);
    layout->addSpacing(45);
    layout->addWidget(titleLbl);
    layout->addSpacing(20);
    layout->addWidget(addressTi);
    layout->addWidget(aliasTi);
    layout->addStretch(1);
}

void BlocknetAddressEdit::clear() {
    //proposalTi->clear();
}