// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetcreateproposal1.h"
#include "blocknethdiv.h"

#include "servicenode-budget.h"
#include "bitcoinunits.h"

#include <QMessageBox>
#include <QKeyEvent>
#include <QDoubleValidator>

BlocknetCreateProposal1::BlocknetCreateProposal1(WalletModel *w, int id, QFrame *parent) : BlocknetCreateProposalPage(w, id, parent),
                                                                                 layout(new QVBoxLayout) {
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(layout);
    layout->setContentsMargins(15, 10, 35, 30);
    layout->setSpacing(0);

    titleLbl = new QLabel(tr("Proposals"));
    titleLbl->setObjectName("h4");

    QLabel *subtitleLbl = new QLabel(tr("Create Proposal"));
    subtitleLbl->setObjectName("h2");

    proposalTi = new BlocknetLineEditWithTitle(tr("Proposal name"), tr("Enter proposal name..."), 675);
    proposalTi->setObjectName("proposal");
    proposalTi->lineEdit->setFocusPolicy(Qt::FocusPolicy::StrongFocus);
    proposalTi->lineEdit->setValidator(new QRegExpValidator(QRegExp("[a-zA-Z0-9]{33,35}"), this));

    urlTi = new BlocknetLineEditWithTitle(tr("URL"), tr("Enter URL..."));
    urlTi->setObjectName("url");

    auto *compactGrid = new QFrame;
    QGridLayout *gridLayout = new QGridLayout;
    gridLayout->setContentsMargins(QMargins());
    gridLayout->setVerticalSpacing(0);
    compactGrid->setLayout(gridLayout);

    paymentCountTi = new BlocknetLineEditWithTitle(tr("Payment count"), tr("Enter payment count..."));
    paymentCountTi->setObjectName("payment");

    superBlockTi = new BlocknetLineEditWithTitle(tr("SuperBlock #"), tr("Enter SuperBlock #..."));
    superBlockTi->setObjectName("block");

    amountTi = new BlocknetLineEditWithTitle(tr("Amount"), tr("Enter amount..."));
    amountTi->setObjectName("amount");

    gridLayout->addWidget(paymentCountTi, 0, 0);
    gridLayout->addWidget(superBlockTi, 0, 1);
    gridLayout->addWidget(amountTi, 1, 0);

    auto *div1 = new BlocknetHDiv;

    auto *feeGrid = new QFrame;
    QGridLayout *feeLayout = new QGridLayout;
    feeLayout->setContentsMargins(15, 0, 15, 0);
    feeGrid->setLayout(feeLayout);

    feeTitleLbl = new QLabel(tr("New Proposal Fee"));
    feeTitleLbl->setObjectName("h4");

    feeLbl = new QLabel(BitcoinUnits::floorWithUnit(0, GetProposalFee(), false, BitcoinUnits::separatorAlways));
    feeLbl->setObjectName("detail");

    feeLayout->addWidget(feeTitleLbl, 0, 0);
    feeLayout->addWidget(feeLbl, 0, 1, Qt::AlignRight);

    auto *div2 = new BlocknetHDiv;

    auto *buttonGrid = new QFrame;
    QGridLayout *buttonLayout = new QGridLayout;
    buttonLayout->setContentsMargins(15, 0, 15, 0);
    buttonLayout->setColumnStretch(0, 0);
    buttonLayout->setColumnStretch(1, 2);
    buttonGrid->setLayout(buttonLayout);

    continueBtn = new BlocknetFormBtn;
    continueBtn->setText(tr("Continue"));
    cancelBtn = new BlocknetFormBtn;
    cancelBtn->setObjectName("cancel");
    cancelBtn->setText(tr("Cancel"));

    buttonLayout->addWidget(cancelBtn, 0, 0, Qt::AlignLeft);
    buttonLayout->addWidget(continueBtn, 0, 1, Qt::AlignLeft);

    layout->addWidget(titleLbl, 0, Qt::AlignTop | Qt::AlignLeft);
    layout->addSpacing(45);
    layout->addWidget(subtitleLbl, 0, Qt::AlignTop);
    layout->addSpacing(20);
    layout->addWidget(proposalTi);
    layout->addWidget(urlTi);
    layout->addWidget(compactGrid);
    layout->addSpacing(20);
    layout->addWidget(div1);
    layout->addSpacing(20);
    layout->addWidget(feeGrid);
    layout->addSpacing(20);
    layout->addWidget(div2);
    layout->addSpacing(40);
    layout->addWidget(buttonGrid);
    layout->addStretch(1);
}

void BlocknetCreateProposal1::keyPressEvent(QKeyEvent *event) {
    QWidget::keyPressEvent(event);
    if (this->isHidden())
        return;
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
        onNext();
}

void BlocknetCreateProposal1::clear() {
    //proposalTi->clear();
}
