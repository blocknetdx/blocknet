// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetcreateproposal3.h"
#include "blocknethdiv.h"

#include "init.h"
#include "util.h"
#include "servicenode-budget.h"
#include "bitcoinunits.h"

#include <QMessageBox>
#include <QKeyEvent>

BlocknetCreateProposal3::BlocknetCreateProposal3(int id, QFrame *parent) : BlocknetCreateProposalPage(id, parent),
                                                                           layout(new QVBoxLayout) {
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(layout);
    layout->setContentsMargins(52, 9, 50, 0);

    titleLbl = new QLabel(tr("Proposals"));
    titleLbl->setObjectName("h4");

    subtitleLbl = new QLabel;
    subtitleLbl->setObjectName("h2");

    auto *descLbl = new QLabel(tr("You are required to wait until your Proposal Submission Fee collateral has finished confirming to the network. You must have at least %1 confirmations.")
                                       .arg(QString::number(BUDGET_FEE_CONFIRMATIONS)));
    descLbl->setObjectName("h6");
    descLbl->setWordWrap(true);

    auto *div1 = new BlocknetHDiv;

    auto *titleGrid = new QFrame;
    QGridLayout *titleLayout = new QGridLayout;
    titleLayout->setContentsMargins(QMargins());
    titleGrid->setLayout(titleLayout);

    proposalTitleLbl = new QLabel(tr("Name"));
    proposalTitleLbl->setObjectName("h5");

    proposalLbl = new QLabel;
    proposalLbl->setObjectName("detail");

    titleLayout->addWidget(proposalTitleLbl, 0, 0);
    titleLayout->addWidget(proposalLbl, 0, 1, Qt::AlignRight);

    auto *div2 = new BlocknetHDiv;

    auto *proposalGrid = new QFrame;
    QGridLayout *proposalLayout = new QGridLayout;
    proposalLayout->setContentsMargins(QMargins());
    proposalGrid->setLayout(proposalLayout);

    proposalDetailTitleLbl = new QLabel(tr("Proposal"));
    proposalDetailTitleLbl->setObjectName("h5");

    proposalDetailLbl = new QLabel;
    proposalDetailLbl->setObjectName("detail");

    proposalLayout->addWidget(proposalDetailTitleLbl, 0, 0);
    proposalLayout->addWidget(proposalDetailLbl, 0, 1, Qt::AlignRight);

    auto *div3 = new BlocknetHDiv;

    auto *feeGrid = new QFrame;
    auto *feeLayout = new QGridLayout;
    feeLayout->setContentsMargins(QMargins());
    feeGrid->setLayout(feeLayout);

    feeTitleLbl = new QLabel(tr("Submission Fee"));
    feeTitleLbl->setObjectName("h5");

    feeLbl = new QLabel(BitcoinUnits::floorWithUnit(0, GetProposalFee(), false, BitcoinUnits::separatorAlways));
    feeLbl->setObjectName("detail");

    feeLayout->addWidget(feeTitleLbl, 0, 0);
    feeLayout->addWidget(feeLbl, 0, 1, Qt::AlignRight);

    auto *div4 = new BlocknetHDiv;

    auto *feeHashGrid = new QFrame;
    auto *feeHashGridLayout = new QGridLayout;
    feeHashGridLayout->setContentsMargins(QMargins());
    feeHashGrid->setLayout(feeHashGridLayout);
    feeHashLbl = new QLabel;
    feeHashLbl->setObjectName("detail");
    feeHashValLbl = new QLabel;
    feeHashValLbl->setObjectName("detail");
    feeHashValLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    feeHashGridLayout->addWidget(feeHashLbl, 0, 0);
    feeHashGridLayout->addWidget(feeHashValLbl, 0, 1, Qt::AlignRight);

    auto *div5 = new BlocknetHDiv;

    // Cancel/continue buttons
    auto *btnBox = new QFrame;
    btnBox->setObjectName("buttonSection");
    auto *btnBoxLayout = new QHBoxLayout;
    btnBoxLayout->setContentsMargins(QMargins());
    btnBoxLayout->setSpacing(15);
    btnBox->setLayout(btnBoxLayout);
    auto *backBtn = new BlocknetFormBtn;
    backBtn->setText(tr("Back"));
    submitBtn = new BlocknetFormBtn;
    submitBtn->setText(tr("Submit Proposal"));
    auto *cancelBtn = new BlocknetFormBtn;
    cancelBtn->setObjectName("cancel");
    cancelBtn->setText(tr("Cancel"));
    btnBoxLayout->addWidget(submitBtn, 0, Qt::AlignLeft | Qt::AlignBottom);
    btnBoxLayout->addStretch(1);
    btnBoxLayout->addWidget(cancelBtn, 0, Qt::AlignRight | Qt::AlignBottom);

    int spacing = 10;

    layout->addWidget(titleLbl, 0, Qt::AlignTop | Qt::AlignLeft);
    layout->addSpacing(45);
    layout->addWidget(subtitleLbl, 0, Qt::AlignTop);
    layout->addSpacing(spacing);
    layout->addWidget(descLbl);
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
    layout->addWidget(feeGrid);
    layout->addSpacing(spacing);
    layout->addWidget(div4);
    layout->addSpacing(spacing);
    layout->addWidget(feeHashGrid);
    layout->addSpacing(spacing);
    layout->addWidget(div5);
    layout->addSpacing(40);
    layout->addStretch(1);
    layout->addWidget(btnBox);
    layout->addSpacing(30);

    connect(submitBtn, SIGNAL(clicked()), this, SLOT(onSubmit()));
    connect(cancelBtn, SIGNAL(clicked()), this, SLOT(onCancel()));
    // Timer used to check for vote capabilities and refresh proposals
    int timerInterval = 30000;
    timer = new QTimer(this);
    timer->setInterval(timerInterval);
    connect(timer, &QTimer::timeout, this, [this]() {
        setModel(this->model);
    });
}

void BlocknetCreateProposal3::setModel(const BlocknetCreateProposalPageModel &model) {
    this->model = model;

    subtitleLbl->setText(tr("Submit Proposal (Superblock %1)").arg(model.superblock));
    feeTitleLbl->setText(tr("Submission Fee Confirmations"));
    feeHashLbl->setText(tr("Submission Fee Hash"));
    feeHashValLbl->setText(QString::fromStdString(model.collateral.ToString()));

    int confs = collateralConfirmations();
    feeLbl->setText(QString("%1 of %2 required")
                            .arg(confs == -1 ? tr("Couldn't find fee collateral") : QString::number(confs),
                                 QString::number(BUDGET_FEE_CONFIRMATIONS)));

    proposalLbl->setText(QString::fromStdString(model.name));
    proposalDetailLbl->setText(tr("%1 payment(s) of %2 for a total of %3").arg(QString::number(model.paymentCount),
            BitcoinUnits::floorWithUnit(BitcoinUnits::BLOCK, model.amount * COIN, false, BitcoinUnits::separatorNever),
            BitcoinUnits::floorWithUnit(BitcoinUnits::BLOCK, model.amount * model.paymentCount * COIN, false, BitcoinUnits::separatorNever)));

    if (!timer->isActive())
        timer->start();
}

void BlocknetCreateProposal3::keyPressEvent(QKeyEvent *event) {
    QWidget::keyPressEvent(event);
    if (this->isHidden())
        return;
    if ((event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) && submitBtn->isEnabled())
        onSubmit();
}

bool BlocknetCreateProposal3::validated() {
    return true;
}

void BlocknetCreateProposal3::onCancel() {
    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Cancel Proposal Submission"),
                                                               tr("Are you sure you want to cancel this proposal submission? You will need to use the debug console to resubmit this proposal later (the command is in your debug.log) otherwise you will forfeit the submission fee"),
                                                               QMessageBox::Yes | QMessageBox::No,
                                                               QMessageBox::No);

    if (retval != QMessageBox::Yes)
        return;

    emit cancel(pageID);
}

void BlocknetCreateProposal3::onSubmit() {
    std::string strError;
    if (!servicenodeSync.IsBlockchainSynced()) {
        QMessageBox::warning(this->parentWidget(), tr("Issue"),
                             tr("Please wait until syncing has finished. Try resubmitting in a few minutes.")
                                     .arg(QString::fromStdString(strError)));
        return;
    }

    CScript scriptPubKey = GetScriptForDestination(model.address.Get());
    CBudgetProposalBroadcast budgetProposalBroadcast(model.name, model.url, model.paymentCount,
                                                     scriptPubKey, model.amount * COIN, model.superblock, model.collateral);

    int nConf = 0;
    if (!IsBudgetCollateralValid(model.collateral, budgetProposalBroadcast.GetHash(), strError, budgetProposalBroadcast.nTime, nConf)) {
        QMessageBox::warning(this->parentWidget(), tr("Issue"),
                             tr("There's a problem with the proposal submission fee: %1").arg(QString::fromStdString(strError)));
        return;
    }

    if (!budget.AddProposal(budgetProposalBroadcast)) {
        QString hash = QString::fromStdString(budgetProposalBroadcast.GetHash().ToString());
        QMessageBox::warning(this->parentWidget(), tr("Issue"),
                             tr("There's a problem with the proposal submission for %1").arg(QString::fromStdString(model.name)));
        return;
    }

    budget.mapSeenServicenodeBudgetProposals.insert(make_pair(budgetProposalBroadcast.GetHash(), budgetProposalBroadcast));
    budgetProposalBroadcast.Relay();

    QString hash = QString::fromStdString(budgetProposalBroadcast.GetHash().ToString());
    QMessageBox::information(this->parentWidget(), tr("Proposal Submitted"),
                             tr("Your proposal was successfully submitted with hash: %1").arg(hash));
    LogPrintf("Proposal %s successfully submitted with hash %s\n", model.name, hash.toStdString());

    emit done();
}

void BlocknetCreateProposal3::clear() {
    timer->stop();
}

int BlocknetCreateProposal3::collateralConfirmations() {
    CScript scriptPubKey = GetScriptForDestination(model.address.Get());
    CBudgetProposalBroadcast budgetProposalBroadcast(model.name, model.url, model.paymentCount,
                                                     scriptPubKey, model.amount * COIN, model.superblock, 0);

    std::string strError;
    int conf = -1;
    IsBudgetCollateralValid(model.collateral, budgetProposalBroadcast.GetHash(), strError,
            budgetProposalBroadcast.nTime, conf);

    return conf;
}
