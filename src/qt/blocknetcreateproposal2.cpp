// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetcreateproposal2.h"
#include "blocknethdiv.h"

#include "init.h"
#include "util.h"
#include "servicenode-budget.h"
#include "bitcoinunits.h"

#include <QMessageBox>
#include <QKeyEvent>

BlocknetCreateProposal2::BlocknetCreateProposal2(int id, QFrame *parent) : BlocknetCreateProposalPage(id, parent),
                                                                           layout(new QVBoxLayout) {
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(layout);
    layout->setContentsMargins(52, 9, 50, 0);

    titleLbl = new QLabel(tr("Proposals"));
    titleLbl->setObjectName("h4");

    subtitleLbl = new QLabel;
    subtitleLbl->setObjectName("h2");

    auto *div1 = new BlocknetHDiv;

    auto *titleGrid = new QFrame;
    QGridLayout *titleLayout = new QGridLayout;
    titleLayout->setContentsMargins(QMargins());
    titleGrid->setLayout(titleLayout);

    proposalTitleLbl = new QLabel(tr("Name"));
    proposalTitleLbl->setObjectName("h4");

    proposalLbl = new QLabel;
    proposalLbl->setObjectName("detail");
    proposalLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);

    titleLayout->addWidget(proposalTitleLbl, 0, 0);
    titleLayout->addWidget(proposalLbl, 0, 1, Qt::AlignRight);

    auto *div2 = new BlocknetHDiv;

    auto *proposalGrid = new QFrame;
    QGridLayout *proposalLayout = new QGridLayout;
    proposalLayout->setContentsMargins(QMargins());
    proposalGrid->setLayout(proposalLayout);

    proposalDetailTitleLbl = new QLabel(tr("Proposal"));
    proposalDetailTitleLbl->setObjectName("h4");

    proposalDetailLbl = new QLabel;
    proposalDetailLbl->setObjectName("detail");
    proposalDetailLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);

    proposalLayout->addWidget(proposalDetailTitleLbl, 0, 0);
    proposalLayout->addWidget(proposalDetailLbl, 0, 1, Qt::AlignRight);

    auto *div3 = new BlocknetHDiv;

    auto *addrGrid = new QFrame;
    auto *addrGridLayout = new QGridLayout;
    addrGridLayout->setContentsMargins(QMargins());
    addrGrid->setLayout(addrGridLayout);
    proposalAddrLbl = new QLabel(tr("Payment Address"));
    proposalAddrLbl->setObjectName("h4");
    proposalAddrValLbl = new QLabel;
    proposalAddrValLbl->setObjectName("detail");
    proposalAddrValLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    addrGridLayout->addWidget(proposalAddrLbl, 0, 0);
    addrGridLayout->addWidget(proposalAddrValLbl, 0, 1, Qt::AlignRight);

    auto *div4 = new BlocknetHDiv;

    auto *urlGrid = new QFrame;
    auto *urlGridLayout = new QGridLayout;
    urlGridLayout->setContentsMargins(QMargins());
    urlGrid->setLayout(urlGridLayout);
    urlLbl = new QLabel(tr("URL"));
    urlLbl->setObjectName("h4");
    urlValLbl = new QLabel;
    urlValLbl->setObjectName("detail");
    urlValLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    urlGridLayout->addWidget(urlLbl, 0, 0);
    urlGridLayout->addWidget(urlValLbl, 0, 1, Qt::AlignRight);

    auto *div5 = new BlocknetHDiv;

    auto *feeGrid = new QFrame;
    auto *feeLayout = new QGridLayout;
    feeLayout->setContentsMargins(QMargins());
    feeGrid->setLayout(feeLayout);

    feeTitleLbl = new QLabel(tr("Submission Fee"));
    feeTitleLbl->setObjectName("h4");

    feeLbl = new QLabel(BitcoinUnits::floorWithUnit(0, GetProposalFee(), false, BitcoinUnits::separatorAlways));
    feeLbl->setObjectName("detail");
    feeLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);

    feeLayout->addWidget(feeTitleLbl, 0, 0);
    feeLayout->addWidget(feeLbl, 0, 1, Qt::AlignRight);

    auto *div6 = new BlocknetHDiv;

    // Cancel/continue buttons
    auto *btnBox = new QFrame;
    btnBox->setObjectName("buttonSection");
    auto *btnBoxLayout = new QHBoxLayout;
    btnBoxLayout->setContentsMargins(QMargins());
    btnBoxLayout->setSpacing(15);
    btnBox->setLayout(btnBoxLayout);
    backBtn = new BlocknetFormBtn;
    backBtn->setText(tr("Back"));
    submitBtn = new BlocknetFormBtn;
    submitBtn->setText(tr("Pay Fee"));
    cancelBtn = new BlocknetFormBtn;
    cancelBtn->setObjectName("cancel");
    cancelBtn->setText(tr("Cancel"));
    btnBoxLayout->addWidget(backBtn, 0, Qt::AlignLeft | Qt::AlignBottom);
    btnBoxLayout->addWidget(submitBtn, 0, Qt::AlignLeft | Qt::AlignBottom);
    btnBoxLayout->addStretch(1);
    btnBoxLayout->addWidget(cancelBtn, 0, Qt::AlignRight | Qt::AlignBottom);

    int spacing = 10;

    layout->addWidget(titleLbl, 0, Qt::AlignTop | Qt::AlignLeft);
    layout->addSpacing(45);
    layout->addWidget(subtitleLbl, 0, Qt::AlignTop);
    layout->addSpacing(spacing);
    layout->addWidget(div1);
    layout->addSpacing(spacing);
    layout->addWidget(titleGrid);
    layout->addSpacing(spacing);
    layout->addWidget(div2);
    layout->addSpacing(spacing);
    layout->addWidget(proposalGrid);
    layout->addSpacing(spacing);
    layout->addWidget(div3);
    layout->addSpacing(spacing);
    layout->addWidget(addrGrid);
    layout->addSpacing(spacing);
    layout->addWidget(div4);
    layout->addSpacing(spacing);
    layout->addWidget(urlGrid);
    layout->addSpacing(spacing);
    layout->addWidget(div5);
    layout->addSpacing(spacing);
    layout->addWidget(feeGrid);
    layout->addSpacing(spacing);
    layout->addWidget(div6);
    layout->addSpacing(40);
    layout->addStretch(1);
    layout->addWidget(btnBox);
    layout->addSpacing(30);

    connect(submitBtn, SIGNAL(clicked()), this, SLOT(onSubmit()));
    connect(cancelBtn, SIGNAL(clicked()), this, SLOT(onCancel()));
    connect(backBtn, SIGNAL(clicked()), this, SLOT(onBack()));
}

void BlocknetCreateProposal2::setModel(const BlocknetCreateProposalPageModel &model) {
    this->model = model;
    subtitleLbl->setText(tr("Review Proposal (Superblock %1)").arg(model.superblock));
    proposalLbl->setText(QString::fromStdString(model.name));
    proposalDetailLbl->setText(tr("%1 payment(s) of %2 for a total of %3").arg(QString::number(model.paymentCount),
            BitcoinUnits::floorWithUnit(BitcoinUnits::BLOCK, model.amount * COIN, false, BitcoinUnits::separatorNever),
            BitcoinUnits::floorWithUnit(BitcoinUnits::BLOCK, model.amount * model.paymentCount * COIN, false, BitcoinUnits::separatorNever)));
    proposalAddrValLbl->setText(QString::fromStdString(model.address.ToString()));
    urlValLbl->setText(QString::fromStdString(model.url));
}

void BlocknetCreateProposal2::keyPressEvent(QKeyEvent *event) {
    QWidget::keyPressEvent(event);
    if (this->isHidden())
        return;
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
        onSubmit();
}

bool BlocknetCreateProposal2::validated() {
    if (model.collateral == uint256()) {
        QMessageBox::warning(this->parentWidget(), tr("Issue"),
                tr("The proposal fee submission is required before proceeding."));
        return false;
    }
    return true;
}

void BlocknetCreateProposal2::onSubmit() {
    disableButtons(true);

    CScript scriptPubKey = GetScriptForDestination(model.address.Get());
    std::string strError;

    CBudgetProposalBroadcast budgetProposalBroadcast(model.name, model.url, model.paymentCount,
                                                     scriptPubKey, model.amount * COIN, model.superblock, 0);

    if (!budgetProposalBroadcast.IsValid(strError, false)) {
        QMessageBox::warning(this->parentWidget(), tr("Issue"),
                             tr("The proposal was rejected, please check all proposal information: %1").arg(QString::fromStdString(strError)));
        disableButtons(false);
        return;
    }

    // Display message box
    auto fee = BitcoinUnits::floorWithUnit(0, GetProposalFee(), false, BitcoinUnits::separatorAlways);
    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm Submission Fee"),
                                                               tr("Are you sure you want to pay the proposal submission fee of %1?\n\nThis cannot be undone").arg(fee),
                                                               QMessageBox::Yes | QMessageBox::Cancel,
                                                               QMessageBox::Cancel);

    if (retval != QMessageBox::Yes) {
        disableButtons(false);
        return;
    }

    CWalletTx wtx;
    if (!pwalletMain->GetBudgetSystemCollateralTX(wtx, budgetProposalBroadcast.GetHash(), false)) {
        QMessageBox::warning(this->parentWidget(), tr("Issue"),
                             tr("Could not submit proposal fee collateral. Please check your wallet balance and make sure your wallet is unlocked."));
        disableButtons(false);
        return;
    }

    // make our change address
    CReserveKey reservekey(pwalletMain);
    // send the tx to the network
    pwalletMain->CommitTransaction(wtx, reservekey, "tx");

    // store collateral hash
    model.collateral = wtx.GetHash();

    LogPrintf("Proposal fee successfully submitted for %s\n", model.name);
    LogPrintf("mnbudget submit %s %s %d %d %s %d %s\n", model.name, model.url, model.paymentCount, model.superblock, model.address.ToString(), model.amount, model.collateral.ToString());

    onNext();

    disableButtons(false);
}

void BlocknetCreateProposal2::clear() {
}

void BlocknetCreateProposal2::disableButtons(const bool &disable) {
    backBtn->setEnabled(!disable);
    submitBtn->setEnabled(!disable);
    cancelBtn->setEnabled(!disable);
}
