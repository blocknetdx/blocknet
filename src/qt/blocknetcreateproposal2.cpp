// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetcreateproposal2.h"
#include "blocknethdiv.h"

#include "servicenode-budget.h"
#include "bitcoinunits.h"

#include <QMessageBox>
#include <QKeyEvent>

BlocknetCreateProposal2::BlocknetCreateProposal2(WalletModel *w, int id, QFrame *parent) : BlocknetCreateProposalPage(w, id, parent),
                                                                                 layout(new QVBoxLayout) {
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(layout);
    layout->setContentsMargins(15, 10, 35, 30);

    titleLbl = new QLabel(tr("Proposals"));
    titleLbl->setObjectName("h4");

    QLabel *subtitleLbl = new QLabel(tr("Review Proposal"));
    subtitleLbl->setObjectName("h2");

    auto *div1 = new BlocknetHDiv;

    auto *titleGrid = new QFrame;
    QGridLayout *titleLayout = new QGridLayout;
    titleLayout->setContentsMargins(15, 0, 15, 0);
    titleGrid->setLayout(titleLayout);

    proposalTitleLbl = new QLabel(tr("Title"));
    proposalTitleLbl->setObjectName("h4");

    proposalLbl = new QLabel(tr("coreteam-p10"));
    proposalLbl->setObjectName("detail");

    titleLayout->addWidget(proposalTitleLbl, 0, 0);
    titleLayout->addWidget(proposalLbl, 0, 1, Qt::AlignRight);

    auto *div2 = new BlocknetHDiv;

    auto *proposalGrid = new QFrame;
    QGridLayout *proposalLayout = new QGridLayout;
    proposalLayout->setContentsMargins(15, 0, 15, 0);
    proposalGrid->setLayout(proposalLayout);

    proposalDetailTitleLbl = new QLabel(tr("Proposal"));
    proposalDetailTitleLbl->setObjectName("h4");

    proposalDetailLbl = new QLabel(tr("2 payment(s) of 500 BLOCK for a total of 1000 BLOCK"));
    proposalDetailLbl->setObjectName("detail");

    proposalLayout->addWidget(proposalDetailTitleLbl, 0, 0);
    proposalLayout->addWidget(proposalDetailLbl, 0, 1, Qt::AlignRight);

    auto *div3 = new BlocknetHDiv;

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

    auto *div4 = new BlocknetHDiv;

    auto *buttonGrid = new QFrame;
    QGridLayout *buttonLayout = new QGridLayout;
    buttonLayout->setContentsMargins(15, 0, 15, 0);
    buttonLayout->setColumnStretch(0, 0);
    buttonLayout->setColumnStretch(1, 2);
    buttonGrid->setLayout(buttonLayout);

    submitBtn = new BlocknetFormBtn;
    submitBtn->setText(tr("Submit"));
    cancelBtn = new BlocknetFormBtn;
    cancelBtn->setObjectName("cancel");
    cancelBtn->setText(tr("Cancel"));

    buttonLayout->addWidget(cancelBtn, 0, 0, Qt::AlignLeft);
    buttonLayout->addWidget(submitBtn, 0, 1, Qt::AlignLeft);

    layout->addWidget(titleLbl, 0, Qt::AlignTop | Qt::AlignLeft);
    layout->addSpacing(45);
    layout->addWidget(subtitleLbl, 0, Qt::AlignTop);
    layout->addSpacing(20);
    layout->addWidget(div1);
    layout->addSpacing(20);
    layout->addWidget(titleGrid);
    layout->addSpacing(20);
    layout->addWidget(div2);
    layout->addSpacing(20);
    layout->addWidget(proposalGrid);
    layout->addSpacing(20);
    layout->addWidget(div3);
    layout->addSpacing(20);
    layout->addWidget(feeGrid);
    layout->addSpacing(20);
    layout->addWidget(div4);
    layout->addSpacing(40);
    layout->addWidget(buttonGrid);
    layout->addStretch(1);
}

void BlocknetCreateProposal2::keyPressEvent(QKeyEvent *event) {
    QWidget::keyPressEvent(event);
    if (this->isHidden())
        return;
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
        onNext();
}

void BlocknetCreateProposal2::clear() {
    /*QLayoutItem *child;
    while ((child = gridLayout->takeAt(0)) != nullptr) {
        delete child;
    }
    QList<QWidget*> list = widgets.toList();
    for (int i = list.count() - 1; i >= 0; --i) {
        QWidget *w = list[i];
        widgets.remove(w);
        w->deleteLater();
    }
    tis.clear();*/
}
