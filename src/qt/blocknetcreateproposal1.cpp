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
#include <QIntValidator>

BlocknetCreateProposal1::BlocknetCreateProposal1(int id, QFrame *parent) : BlocknetCreateProposalPage(id, parent),
                                                                           layout(new QVBoxLayout) {
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(layout);
    layout->setContentsMargins(52, 9, 50, 0);
    layout->setSpacing(0);

    titleLbl = new QLabel(tr("Proposals"));
    titleLbl->setObjectName("h4");

    QLabel *subtitleLbl = new QLabel(tr("Create Proposal"));
    subtitleLbl->setObjectName("h2");

    proposalTi = new BlocknetLineEditWithTitle(tr("Proposal name (max 50 characters)"), tr("Enter proposal name..."), 675);
    proposalTi->setObjectName("proposal");
    proposalTi->lineEdit->setFocusPolicy(Qt::FocusPolicy::StrongFocus);
    proposalTi->lineEdit->setValidator(new QRegExpValidator(QRegExp("[a-zA-Z0-9-_]+"), this));
    proposalTi->lineEdit->setMaxLength(50);

    urlTi = new BlocknetLineEditWithTitle(tr("URL (max 150 characters)"), tr("Enter URL..."));
    urlTi->setObjectName("url");
    urlTi->lineEdit->setValidator(new QRegExpValidator(QRegExp("https?:\\/\\/(www\\.)?[-a-zA-Z0-9@:%._\\+~#=]{2,256}\\.[a-z]{2,6}\\b([-a-zA-Z0-9@:%_\\+.~#?&//=]*)"), this));
    urlTi->lineEdit->setMaxLength(150);

    auto *compactGrid = new QFrame;
    QGridLayout *gridLayout = new QGridLayout;
    gridLayout->setContentsMargins(QMargins());
    gridLayout->setVerticalSpacing(8);
    compactGrid->setLayout(gridLayout);

    paymentCountTi = new BlocknetLineEditWithTitle(tr("Payment count (1-12)"), tr("Enter payment count..."));
    paymentCountTi->setObjectName("payment");
    paymentCountTi->lineEdit->setValidator(new QIntValidator(1, 12));
    paymentCountTi->lineEdit->setText("1");

    auto superblock = nextSuperblock();
    auto superblockStr = superblock == -1 ? QString() : QString::number(superblock);
    superBlockTi = new BlocknetLineEditWithTitle(tr("Superblock #: Next is %1").arg(superblockStr), tr("Enter Superblock #..."));
    superBlockTi->setObjectName("block");
    superBlockTi->lineEdit->setValidator(new QIntValidator(1, 100000000));
    superBlockTi->lineEdit->setText(superblockStr);

    amountTi = new BlocknetLineEditWithTitle(tr("Amount (10 minimum)"), tr("Enter amount..."));
    amountTi->setObjectName("amount");
    amountTi->lineEdit->setValidator(new QRegExpValidator(QRegExp("[0-9]{2,4}"), this));
    amountTi->lineEdit->setMaxLength(
            static_cast<int>(boost::lexical_cast<std::string>(CBudgetManager::GetTotalBudget(0) / COIN).length()));

    paymentAddrTi = new BlocknetLineEditWithTitle(tr("Payment address"), tr("Enter payment address..."));
    paymentAddrTi->setObjectName("address");
    paymentAddrTi->lineEdit->setValidator(new QRegExpValidator(QRegExp("[a-zA-Z0-9]{33,35}"), this));
    paymentAddrTi->lineEdit->setMaxLength(35);

    gridLayout->addWidget(paymentCountTi, 0, 0);
    gridLayout->addWidget(superBlockTi, 0, 1);
    gridLayout->addWidget(amountTi, 1, 0);
    gridLayout->addWidget(paymentAddrTi, 1, 1);

    auto *div1 = new BlocknetHDiv;

    auto *feeGrid = new QFrame;
    auto *feeLayout = new QGridLayout;
    feeLayout->setContentsMargins(QMargins());
    feeGrid->setLayout(feeLayout);

    feeTitleLbl = new QLabel(tr("New Proposal Fee"));
    feeTitleLbl->setObjectName("h4");

    feeLbl = new QLabel(BitcoinUnits::floorWithUnit(0, GetProposalFee(), false, BitcoinUnits::separatorAlways));
    feeLbl->setObjectName("detail");

    feeLayout->addWidget(feeTitleLbl, 0, 0);
    feeLayout->addWidget(feeLbl, 0, 1, Qt::AlignRight);

    auto *div2 = new BlocknetHDiv;

    auto *buttonGrid = new QFrame;
    buttonGrid->setContentsMargins(QMargins());
    QGridLayout *buttonLayout = new QGridLayout;
    buttonLayout->setContentsMargins(QMargins());
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
    layout->addSpacing(10);
    layout->addWidget(urlTi);
    layout->addSpacing(10);
    layout->addWidget(compactGrid);
    layout->addSpacing(30);
    layout->addWidget(div1);
    layout->addSpacing(20);
    layout->addWidget(feeGrid);
    layout->addSpacing(20);
    layout->addWidget(div2);
    layout->addSpacing(40);
    layout->addStretch(1);
    layout->addWidget(buttonGrid);
    layout->addSpacing(15);

    connect(continueBtn, SIGNAL(clicked()), this, SLOT(onNext()));
    connect(cancelBtn, SIGNAL(clicked()), this, SLOT(onCancel()));
}

bool BlocknetCreateProposal1::validated() {
    bool empty = proposalTi->isEmpty()
        && urlTi->isEmpty()
        && paymentCountTi->isEmpty()
        && superBlockTi->isEmpty()
        && amountTi->isEmpty()
        && paymentAddrTi->isEmpty();
    if (empty) {
        QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Please fill out the entire form"));
        return false;
    }

    auto url = urlTi->lineEdit->text();
    QRegularExpression re("https?:\\/\\/(www\\.)?[-a-zA-Z0-9@:%._\\+~#=]{2,256}\\.[a-z]{2,6}\\b([-a-zA-Z0-9@:%_\\+.~#?&//=]*)");
    if (!re.match(url).hasMatch()) {
        QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Bad proposal url"));
        return false;
    }

    auto paymentCount = boost::lexical_cast<int>(paymentCountTi->lineEdit->text().toStdString());
    if (paymentCount == 0 || paymentCount > 12) { // bad payment count
        QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Payment count cannot be 0"));
        return false;
    }

    auto superblock = boost::lexical_cast<int>(superBlockTi->lineEdit->text().toStdString());
    if (superblock % GetBudgetPaymentCycleBlocks() != 0) { // bad superblock
        QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Bad Superblock #"));
        return false;
    }

    auto amount = boost::lexical_cast<int>(amountTi->lineEdit->text().toStdString());
    if (amount < 10) { // amount is too small
        QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Amount is too small, please specify an amount of 10 or more"));
        return false;
    }
    if (amount > CBudgetManager::GetTotalBudget(0)/COIN) { // amount is too large
        QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Amount is too large, maximum amount is %1").arg(CBudgetManager::GetTotalBudget(0)/COIN));
        return false;
    }

    auto proposalAddr = paymentAddrTi->lineEdit->text().toStdString();
    CBitcoinAddress address(proposalAddr);
    if (!address.IsValid()) {
        QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Bad proposal address"));
        return false;
    }

    return true;
}

void BlocknetCreateProposal1::focusInEvent(QFocusEvent *event) {
    QWidget::focusInEvent(event);
    proposalTi->setFocus();
}

void BlocknetCreateProposal1::keyPressEvent(QKeyEvent *event) {
    QWidget::keyPressEvent(event);
    if (this->isHidden())
        return;
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
        onNext();
}

void BlocknetCreateProposal1::clear() {
    proposalTi->lineEdit->clear();
    urlTi->lineEdit->clear();
    paymentCountTi->lineEdit->clear();
    superBlockTi->lineEdit->clear();
    amountTi->lineEdit->clear();
    paymentAddrTi->lineEdit->clear();
}

int BlocknetCreateProposal1::nextSuperblock() {
    CBlockIndex *pindexPrev = chainActive.Tip();
    if (!pindexPrev)
        return -1;

    int nNext = pindexPrev->nHeight - pindexPrev->nHeight % GetBudgetPaymentCycleBlocks() + GetBudgetPaymentCycleBlocks();
    return nNext;
}