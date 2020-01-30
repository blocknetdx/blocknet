// Copyright (c) 2018-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetcreateproposal2.h>

#include <qt/blocknetguiutil.h>
#include <qt/blocknethdiv.h>

#include <qt/bitcoinunits.h>

#include <chainparams.h>
#include <governance/governancewallet.h>
#include <net.h>
#include <wallet/wallet.h>

#include <QKeyEvent>
#include <QMessageBox>

BlocknetCreateProposal2::BlocknetCreateProposal2(int id, QFrame *parent) : BlocknetCreateProposalPage(id, parent),
                                                                           layout(new QVBoxLayout) {
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(layout);
    layout->setContentsMargins(BGU::spi(52), BGU::spi(9), BGU::spi(50), 0);

    titleLbl = new QLabel(tr("Proposals"));
    titleLbl->setObjectName("h4");

    subtitleLbl = new QLabel;
    subtitleLbl->setObjectName("h2");

    auto *div1 = new BlocknetHDiv;

    auto *titleGrid = new QFrame;
    auto *titleLayout = new QGridLayout;
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
    auto *proposalLayout = new QGridLayout;
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

    feeLbl = new QLabel(BitcoinUnits::floorWithUnit(0, Params().GetConsensus().proposalFee, 2, false, BitcoinUnits::separatorAlways));
    feeLbl->setObjectName("detail");
    feeLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);

    feeLayout->addWidget(feeTitleLbl, 0, 0);
    feeLayout->addWidget(feeLbl, 0, 1, Qt::AlignRight);

    auto *div6 = new BlocknetHDiv;

    auto *descGrid = new QFrame;
    auto *descLayout = new QVBoxLayout;
    descLayout->setContentsMargins(QMargins());
    descGrid->setLayout(descLayout);

    descLbl = new QLabel(tr("Description"));
    descLbl->setObjectName("h4");

    descValLbl = new QLabel;
    descValLbl->setObjectName("detail");
    descValLbl->setWordWrap(true);
    descValLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);

    descLayout->addWidget(descLbl);
    descLayout->addWidget(descValLbl);

    auto *div7 = new BlocknetHDiv;

    // Cancel/continue buttons
    auto *btnBox = new QFrame;
    btnBox->setObjectName("buttonSection");
    auto *btnBoxLayout = new QHBoxLayout;
    btnBoxLayout->setContentsMargins(QMargins());
    btnBoxLayout->setSpacing(BGU::spi(15));
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
    layout->addSpacing(BGU::spi(25));
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
    layout->addWidget(descGrid);
    layout->addSpacing(spacing);
    layout->addWidget(div7);
    layout->addSpacing(BGU::spi(20));
    layout->addStretch(1);
    layout->addWidget(btnBox);
    layout->addSpacing(BGU::spi(20));

    connect(submitBtn, &BlocknetFormBtn::clicked, this, &BlocknetCreateProposal2::onSubmit);
    connect(cancelBtn, &BlocknetFormBtn::clicked, this, &BlocknetCreateProposal2::onCancel);
    connect(backBtn, &BlocknetFormBtn::clicked, this, &BlocknetCreateProposal2::onBack);
}

void BlocknetCreateProposal2::setModel(const BlocknetCreateProposalPageModel & m) {
    this->model = m;
    subtitleLbl->setText(tr("Review Proposal (Superblock %1)").arg(model.superblock));
    proposalLbl->setText(QString::fromStdString(model.name));
    proposalDetailLbl->setText(tr("Total payment of %1").arg(BitcoinUnits::floorWithUnit(BitcoinUnit::BTC,
            model.amount * COIN, 2, false, BitcoinUnits::separatorNever)));
    proposalAddrValLbl->setText(QString::fromStdString(EncodeDestination(model.address)));
    urlValLbl->setText(QString::fromStdString(model.url));
    descValLbl->setText(QString::fromStdString(model.description));
}

void BlocknetCreateProposal2::keyPressEvent(QKeyEvent *event) {
    QWidget::keyPressEvent(event);
    if (this->isHidden())
        return;
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
        onSubmit();
}

bool BlocknetCreateProposal2::validated() {
    gov::Proposal proposal(model.name, model.superblock, model.amount*COIN, EncodeDestination(model.address),
                           model.url, model.description);
    std::string failureReason;
    if (!proposal.isValid(Params().GetConsensus(), &failureReason)) {
        QMessageBox::warning(this->parentWidget(), tr("Issue"), tr(failureReason.c_str()));
        return false;
    }
    return true;
}

void BlocknetCreateProposal2::onSubmit() {
    disableButtons(true);
    if (!validated())
        disableButtons(false);

    // Display message box
    auto fee = BitcoinUnits::floorWithUnit(0, Params().GetConsensus().proposalFee, 2, false, BitcoinUnits::separatorAlways);
    auto retval = static_cast<QMessageBox::StandardButton>(QMessageBox::question(this, tr("Confirm Submission Fee"),
            tr("Are you sure you want to pay the proposal submission fee of %1?\n\nThis cannot be undone").arg(fee),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel));

    if (retval != QMessageBox::Yes) {
        disableButtons(false);
        return;
    }

    WalletModel::UnlockContext ctx(walletModel->requestUnlock());
    if (!ctx.isValid()) {
        disableButtons(false);
        return;
    }

    gov::Proposal proposal(model.name, model.superblock, model.amount*COIN, EncodeDestination(model.address),
            model.url, model.description);
    CTransactionRef tx;
    std::string failureReason;
    if (!gov::SubmitProposal(proposal, GetWallets(), Params().GetConsensus(), tx, g_connman.get(), &failureReason)) {
        QMessageBox::warning(this->parentWidget(), tr("Issue"),
                tr("The proposal was rejected: %1").arg(QString::fromStdString(failureReason)));
        disableButtons(false);
        return;
    }

    // Assign the fee hash
    model.feehash = tx->GetHash();

    LogPrintf("Proposal %s was successfully submitted\n", proposal.getName());

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
