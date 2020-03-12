// Copyright (c) 2018-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetcreateproposal3.h>

#include <qt/blocknetguiutil.h>
#include <qt/blocknethdiv.h>

#include <qt/bitcoinunits.h>

#include <chainparams.h>
#include <validation.h>

#include <QKeyEvent>
#include <QMessageBox>

BlocknetCreateProposal3::BlocknetCreateProposal3(int id, QFrame *parent) : BlocknetCreateProposalPage(id, parent),
                                                                           layout(new QVBoxLayout)
{
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(layout);
    layout->setContentsMargins(BGU::spi(52), BGU::spi(9), BGU::spi(50), 0);

    titleLbl = new QLabel(tr("Proposals"));
    titleLbl->setObjectName("h4");

    subtitleLbl = new QLabel;
    subtitleLbl->setObjectName("h2");

    auto *descLbl = new QLabel(tr("Your proposal was successfully submitted."));
    descLbl->setObjectName("h6");
    descLbl->setWordWrap(true);

    auto *div1 = new BlocknetHDiv;

    auto *titleGrid = new QFrame;
    auto *titleLayout = new QGridLayout;
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
    auto *proposalLayout = new QGridLayout;
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

    feeLbl = new QLabel(BitcoinUnits::floorWithUnit(0, Params().GetConsensus().proposalFee, 2, false, BitcoinUnits::separatorAlways));
    feeLbl->setObjectName("detail");

    feeLayout->addWidget(feeTitleLbl, 0, 0);
    feeLayout->addWidget(feeLbl, 0, 1, Qt::AlignRight);

    auto *div4 = new BlocknetHDiv;

    auto *feeHashGrid = new QFrame;
    auto *feeHashGridLayout = new QGridLayout;
    feeHashGridLayout->setContentsMargins(QMargins());
    feeHashGrid->setLayout(feeHashGridLayout);
    feeHashLbl = new QLabel;
    feeHashLbl->setObjectName("h5");
    feeHashValLbl = new QLabel;
    feeHashValLbl->setObjectName("detail");
    feeHashValLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    feeHashGridLayout->addWidget(feeHashLbl, 0, 0);
    feeHashGridLayout->addWidget(feeHashValLbl, 1, 0);

    auto *div5 = new BlocknetHDiv;

    // Cancel/continue buttons
    auto *btnBox = new QFrame;
    btnBox->setObjectName("buttonSection");
    auto *btnBoxLayout = new QHBoxLayout;
    btnBoxLayout->setContentsMargins(QMargins());
    btnBoxLayout->setSpacing(BGU::spi(15));
    btnBox->setLayout(btnBoxLayout);
    doneBtn = new BlocknetFormBtn;
    doneBtn->setText(tr("Done"));
    btnBoxLayout->addWidget(doneBtn, 0, Qt::AlignCenter | Qt::AlignBottom);
    btnBoxLayout->addStretch(1);

    int spacing = BGU::spi(10);

    layout->addWidget(titleLbl, 0, Qt::AlignTop | Qt::AlignLeft);
    layout->addSpacing(BGU::spi(25));
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
    layout->addSpacing(BGU::spi(20));
    layout->addStretch(1);
    layout->addWidget(btnBox);
    layout->addSpacing(BGU::spi(20));

    connect(doneBtn, &BlocknetFormBtn::clicked, this, &BlocknetCreateProposal3::onSubmit);
    // Timer used to check for vote capabilities and refresh proposals
    int timerInterval = 30000;
    timer = new QTimer(this);
    timer->setInterval(timerInterval);
    connect(timer, &QTimer::timeout, this, [this]() {
        setModel(this->model);
    });
}

void BlocknetCreateProposal3::setModel(const BlocknetCreateProposalPageModel & m) {
    this->model = m;

    subtitleLbl->setText(tr("Superblock %1").arg(model.superblock));
    feeTitleLbl->setText(tr("Proposal Confirmations"));
    feeHashLbl->setText(tr("Proposal Fee Hash"));
    feeHashValLbl->setText(QString::fromStdString(model.feehash.ToString()));

    int confs = collateralConfirmations();
    feeLbl->setText(QString("%1").arg(confs == -1 ? tr("Proposal submission hasn't confirmed yet")
                                                  : QString::number(confs)));

    proposalLbl->setText(QString::fromStdString(model.name));
    proposalDetailLbl->setText(tr("Total payment of %1").arg(BitcoinUnits::floorWithUnit(BitcoinUnits::BTC,
            model.amount * COIN, 2, false, BitcoinUnits::separatorNever)));

    if (!timer->isActive())
        timer->start();
}

void BlocknetCreateProposal3::keyPressEvent(QKeyEvent *event) {
    QWidget::keyPressEvent(event);
    if (this->isHidden())
        return;
    if ((event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) && doneBtn->isEnabled())
        onSubmit();
}

bool BlocknetCreateProposal3::validated() {
    return true;
}

void BlocknetCreateProposal3::onCancel() {
    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Cancel Proposal Submission"),
            tr("Are you sure you want to cancel this proposal submission? You will need to use the debug console to "
               "resubmit this proposal later (the command is in your debug.log) otherwise you will forfeit the "
               "submission fee"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (retval != QMessageBox::Yes)
        return;

    Q_EMIT cancel(pageID);
}

void BlocknetCreateProposal3::onSubmit() {
    Q_EMIT done();
}

void BlocknetCreateProposal3::clear() {
    timer->stop();
}

int BlocknetCreateProposal3::collateralConfirmations() {
    CTransactionRef tx;
    uint256 block;
    if (!GetTransaction(model.feehash, tx, Params().GetConsensus(), block))
        return -1;

    LOCK(cs_main);
    CBlockIndex *pindex = LookupBlockIndex(block);
    if (pindex)
        return chainActive.Height() - pindex->nHeight + 1; // tip height - proposal height = 1 conf

    return -1;
}
