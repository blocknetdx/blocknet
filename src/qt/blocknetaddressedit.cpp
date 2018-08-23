// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetaddressedit.h"
#include "blocknethdiv.h"

#include <QMessageBox>
#include <QKeyEvent>
#include <QDoubleValidator>

BlocknetAddressEdit::BlocknetAddressEdit(WalletModel *w, QString t, QString b, QFrame *parent) : QFrame(parent), title(t), buttonString(b), walletModel(w),
                                                                                 layout(new QVBoxLayout) {
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(layout);
    layout->setContentsMargins(46, 10, 50, 30);
    layout->setSpacing(0);

    addressLbl = new QLabel(tr("Address Book"));
    addressLbl->setObjectName("h4");

    QLabel *titleLbl = new QLabel(title);
    titleLbl->setObjectName("h2");

    addressTi = new BlocknetLineEditWithTitle(tr("Address"), tr("Enter Address..."));
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

    connect(addressTi, SIGNAL(textChanged()), this, SLOT(addressChanged()));
    connect(aliasTi, SIGNAL(textChanged()), this, SLOT(aliasChanged()));
    connect(confirmBtn, SIGNAL(clicked()), this, SLOT(onApply()));
}

void BlocknetAddressEdit::clear() {
    addressTi->lineEdit->clear();
    aliasTi->lineEdit->clear();
}

void BlocknetAddressEdit::focusInEvent(QFocusEvent *event) {
    QWidget::focusInEvent(event);
    addressTi->setFocus();
}

void BlocknetAddressEdit::keyPressEvent(QKeyEvent *event) {
    QWidget::keyPressEvent(event);
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
        onApply();
}

bool BlocknetAddressEdit::validated() {
    return true;
}

void BlocknetAddressEdit::addressChanged() {
    //auto addresses = addressTi->getAddresses();
}

void BlocknetAddressEdit::aliasChanged() {
    //auto addresses = aliasTi->getAddresses();
}