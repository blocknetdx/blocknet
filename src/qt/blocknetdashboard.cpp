// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetdashboard.h"
#include "blocknetvars.h"
#include "blockneticonbtn.h"
#include "blocknethdiv.h"

#include "bitcoinunits.h"
#include "optionsmodel.h"

#include <QVariant>
#include <QDebug>

BlocknetDashboard::BlocknetDashboard(QFrame *parent) : QFrame(parent), layout(new QVBoxLayout),
                                                       walletModel(nullptr),
                                                       displayUnit(0), walletBalance(0),
                                                       unconfirmedBalance(0), immatureBalance(0) {
//    this->setStyleSheet("border: 1px solid red");
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->setContentsMargins(46, 10, 50, 0);
    this->setLayout(layout);

    titleLbl = new QLabel(tr("Dashboard"));
    titleLbl->setObjectName("h4");

    auto *balanceGrid = new QFrame;
    auto *balanceGridLayout = new QGridLayout;
    balanceGridLayout->setContentsMargins(QMargins());
    balanceGrid->setLayout(balanceGridLayout);

    auto *balanceBox = new QFrame;
    auto *balanceBoxLayout = new QVBoxLayout;
    balanceBoxLayout->setContentsMargins(QMargins());
    balanceBox->setLayout(balanceBoxLayout);
    balanceLbl = new QLabel(tr("Blocknet Balance"));
    balanceLbl->setObjectName("balanceLbl");
    balanceValueLbl = new QLabel;
    balanceValueLbl->setObjectName("h1");

    auto *pendingBox = new QFrame;
    auto *pendingLayout = new QHBoxLayout;
    pendingLayout->setContentsMargins(QMargins());
    pendingBox->setLayout(pendingLayout);
    pendingLbl = new QLabel(tr("Pending:"));
    pendingLbl->setObjectName("pendingLbl");
    pendingValueLbl = new QLabel;
    pendingValueLbl->setObjectName("pendingValueLbl");
    pendingLayout->addWidget(pendingLbl);
    pendingLayout->addWidget(pendingValueLbl, 0, Qt::AlignLeft);
    pendingLayout->addStretch(1);

    auto *immatureBox = new QFrame;
    auto *immatureLayout = new QHBoxLayout;
    immatureLayout->setContentsMargins(QMargins());
    immatureBox->setLayout(immatureLayout);
    immatureLbl = new QLabel(tr("Immature:"));
    immatureLbl->setObjectName("immatureLbl");
    immatureValueLbl = new QLabel;
    immatureValueLbl->setObjectName("immatureValueLbl");
    immatureLayout->addWidget(immatureLbl);
    immatureLayout->addWidget(immatureValueLbl, 0, Qt::AlignLeft);
    immatureLayout->addStretch(1);

    auto *totalBox = new QFrame;
    auto *totalLayout = new QHBoxLayout;
    totalLayout->setContentsMargins(QMargins());
    totalBox->setLayout(totalLayout);
    totalLbl = new QLabel(tr("Total:"));
    totalLbl->setObjectName("totalLbl");
    totalValueLbl = new QLabel;
    totalValueLbl->setObjectName("totalValueLbl");
    totalLayout->addWidget(totalLbl);
    totalLayout->addWidget(totalValueLbl, 0, Qt::AlignLeft);
    totalLayout->addStretch(1);

    balanceBoxLayout->addWidget(balanceValueLbl);
    balanceBoxLayout->addWidget(pendingBox);
    balanceBoxLayout->addWidget(immatureBox);
    balanceBoxLayout->addWidget(totalBox);

    auto *quickSend = new BlocknetIconBtn(tr("Quick Send"), ":/redesign/QuickActions/QuickSendIcon.png");

    balanceGridLayout->addWidget(balanceBox, 0, 0, Qt::AlignLeft);
    balanceGridLayout->addWidget(quickSend, 0, 1, Qt::AlignRight | Qt::AlignVCenter);
    balanceGridLayout->setColumnStretch(1, 1);

    auto *hdiv = new BlocknetHDiv;

    auto *recentBox = new QFrame;
    auto *recentLayout = new QHBoxLayout;
    recentLayout->setContentsMargins(QMargins());
    recentLayout->setSpacing(0);
    recentBox->setLayout(recentLayout);
    recentTxsLbl = new QLabel(tr("Recent Transactions"));
    recentTxsLbl->setObjectName("recentTransactionsLbl");
    viewAll = new QPushButton;
    viewAll->setObjectName("linkBtn");
    viewAll->setText(tr("View All"));
    viewAll->setCursor(Qt::PointingHandCursor);
    recentLayout->addWidget(recentTxsLbl, 0, Qt::AlignBottom);
    recentLayout->addWidget(viewAll, 0, Qt::AlignRight | Qt::AlignBottom);

    recentTransactionsGrid = new QFrame;
    recentTransactionsGrid->setObjectName("content");
    auto *recentTransactionsGridLayout = new QGridLayout;
    recentTransactionsGridLayout->setContentsMargins(QMargins());
    recentTransactionsGridLayout->setSpacing(0);
    recentTransactionsGrid->setLayout(recentTransactionsGridLayout);
    recentTransactionsGridScrollArea = new QScrollArea;
    recentTransactionsGridScrollArea->setWidgetResizable(true);
    recentTransactionsGridScrollArea->setWidget(recentTransactionsGrid);

    layout->addWidget(titleLbl, 0, Qt::AlignTop | Qt::AlignLeft);
    layout->addWidget(balanceLbl);
    layout->addWidget(balanceGrid);
    layout->addSpacing(40);
    layout->addWidget(hdiv);
    layout->addSpacing(30);
    layout->addWidget(recentBox);
    layout->addWidget(recentTransactionsGridScrollArea, 1);
    layout->addSpacing(30);

    connect(quickSend, SIGNAL(clicked()), this, SLOT(onQuickSend()));
    connect(viewAll, SIGNAL(clicked()), this, SLOT(onViewAll()));
}

void BlocknetDashboard::setWalletModel(WalletModel *w) {
    if (walletModel == w)
        return;

    walletEvents(false);

    walletModel = w;
    if (!walletModel || !walletModel->getOptionsModel())
        return;

    displayUnit = walletModel->getOptionsModel()->getDisplayUnit();
    balanceChanged(walletModel->getBalance(), walletModel->getUnconfirmedBalance(), walletModel->getImmatureBalance(), walletModel->getAnonymizedBalance(),
                   walletModel->getWatchBalance(), walletModel->getWatchUnconfirmedBalance(), walletModel->getWatchImmatureBalance());

    // Watch for wallet changes
    walletEvents(true);
}

/**
 * Draws the transactions into the recent transactions grid. Existing items are removed
 * from the grid layout prior to adding new transactions.
 * @param transactions
 */
void BlocknetDashboard::setRecentTransactions(QVector<BlocknetRecentTransaction> transactions) {
    this->transactions = transactions;

    // Sort descending by date (most recent transaction at [0])
    std::sort(this->transactions.begin(), this->transactions.end(), [](const BlocknetRecentTransaction &a, const BlocknetRecentTransaction &b) {
       return a.date > b.date;
    });

    auto *l = (QGridLayout*)recentTransactionsGrid->layout();
    QLayoutItem *c;
    while ((c = l->takeAt(0)) != nullptr) {
        c->widget()->hide();
        c->widget()->deleteLater();
        delete c;
    }

    // Hide the grid if there are no transactions
    if (this->transactions.empty())
        recentTransactionsGrid->hide();
    else recentTransactionsGrid->show();

    int rowh = 65;
    int i = 0;
    for (i; i < this->transactions.count(); ++i) {
        BlocknetRecentTransaction &rt = this->transactions[i];
        bool rowA = i % 2 == 0; // used for grid background colors

        // 1st column (indicator)
        auto *indicatorBox = new QFrame;
        indicatorBox->setObjectName("indicator");
        indicatorBox->setProperty("state", QVariant(rt.status));
        indicatorBox->setFixedWidth(3);
        indicatorBox->setToolTip(rt.tooltip);
        l->addWidget(indicatorBox, i, 0);

        // 2nd column (date)
        auto *dateBox = new QFrame;
        dateBox->setObjectName("dateBox");
        dateBox->setProperty("rowA", QVariant(rowA));
        dateBox->setFixedSize(80, rowh);
        auto *dateLayout = new QVBoxLayout;
        dateLayout->setContentsMargins(QMargins());
        dateLayout->setSpacing(0);
        dateBox->setLayout(dateLayout);
        QLabel *monthLbl = new QLabel; monthLbl->setObjectName("month"); monthLbl->setAlignment(Qt::AlignCenter); monthLbl->setText(rt.date.toString("MMM").toUpper());
        QLabel *dateLbl = new QLabel; dateLbl->setObjectName("date"); dateLbl->setAlignment(Qt::AlignCenter); dateLbl->setText(rt.date.toString("dd"));
        dateLayout->addWidget(monthLbl, 0, Qt::AlignBottom);
        dateLayout->addWidget(dateLbl, 0, Qt::AlignTop);
        dateLayout->addSpacing(10);
        dateBox->setToolTip(rt.tooltip);
        l->addWidget(dateBox, i, 1);

        // 3rd column
        QLabel *timeLbl = new QLabel; timeLbl->setObjectName("time"); timeLbl->setText(rt.date.toString("h:mmap")); timeLbl->setFixedWidth(80);
        timeLbl->setProperty("rowA", QVariant(rowA));
        timeLbl->setFixedHeight(rowh);
        timeLbl->setToolTip(rt.tooltip);
        l->addWidget(timeLbl, i, 2);

        // 4th column
        QLabel *aliasLbl = new QLabel; aliasLbl->setObjectName("alias"); aliasLbl->setText(rt.alias);
        aliasLbl->setProperty("rowA", QVariant(rowA));
        aliasLbl->setFixedHeight(rowh);
        aliasLbl->setToolTip(rt.tooltip);
        l->addWidget(aliasLbl, i, 3);
        l->setColumnStretch(3, 3);

        // 5th column
        QLabel *actionLbl = new QLabel; actionLbl->setObjectName("action"); actionLbl->setText(rt.type);
        actionLbl->setProperty("rowA", QVariant(rowA));
        actionLbl->setFixedHeight(rowh);
        actionLbl->setToolTip(rt.tooltip);
        l->addWidget(actionLbl, i, 4);
        l->setColumnStretch(4, 1);

        // 6th column
        QString amountTxt;
        QLabel *amountLbl = new QLabel; amountLbl->setObjectName("amount");
        amountLbl->setProperty("rowA", QVariant(rowA));
        amountLbl->setProperty("positive", QVariant(rt.amount >= 0));
        QString prefix = rt.amount < 0 ? QString("-") : QString("");
        amountLbl->setText(QString("%1%2").arg(prefix, BitcoinUnits::formatWithUnit(displayUnit, rt.amount, true, BitcoinUnits::separatorNever)));
        amountLbl->setFixedHeight(rowh);
        amountLbl->setToolTip(rt.tooltip);
        l->addWidget(amountLbl, i, 5);

        // 7th column
//        QLabel *moreLbl = new QLabel; moreLbl->setObjectName("more"); moreLbl->setText("• • •"); moreLbl->setFixedWidth(80); moreLbl->setAlignment(Qt::AlignCenter);
//        moreLbl->setProperty("rowA", QVariant(rowA));
//        moreLbl->setFixedHeight(rowh);
//        l->addWidget(moreLbl, i, 6);

        // Grid layout attributes
        l->setRowMinimumHeight(i, rowh);
        l->setRowStretch(i, 0);
    }
    auto *stretchLbl = new QLabel;
    stretchLbl->setObjectName("stetch");
    l->addWidget(stretchLbl, i + 1, 0, 1, 6);
    l->setRowStretch(i + 1, 2);
}

void BlocknetDashboard::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    walletEvents(true);
}

void BlocknetDashboard::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    walletEvents(false);
}

void BlocknetDashboard::balanceChanged(const CAmount balance, const CAmount unconfirmed, const CAmount immature,
                                       const CAmount anonymized, const CAmount watch, const CAmount watchUnconfirmed,
                                       const CAmount watchImmature) {
    walletBalance = balance;
    unconfirmedBalance = unconfirmed;
    immatureBalance = immature;
    updateBalance();
}

void BlocknetDashboard::displayUnitChanged(int unit) {
    displayUnit = unit;
    updateBalance();
    setRecentTransactions(this->transactions);
};

void BlocknetDashboard::updateBalance() {
    balanceValueLbl->setText(BitcoinUnits::formatWithUnit(displayUnit, walletBalance - immatureBalance, false, BitcoinUnits::separatorAlways));
    pendingValueLbl->setText(BitcoinUnits::formatWithUnit(displayUnit, unconfirmedBalance, false, BitcoinUnits::separatorAlways));
    immatureValueLbl->setText(BitcoinUnits::formatWithUnit(displayUnit, immatureBalance, false, BitcoinUnits::separatorAlways));
    totalValueLbl->setText(BitcoinUnits::formatWithUnit(displayUnit, walletBalance + unconfirmedBalance, false, BitcoinUnits::separatorAlways));
};

void BlocknetDashboard::onRecentTransactions(QVector<BlocknetDashboard::BlocknetRecentTransaction> &txs) {
    setRecentTransactions(txs);
}

void BlocknetDashboard::walletEvents(bool on) {
    if (walletModel && on) {
        connect(walletModel, SIGNAL(balanceChanged(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)),
                this, SLOT(balanceChanged(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)));
        connect(walletModel->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(displayUnitChanged(int)));
    } else if (walletModel) {
        disconnect(walletModel, SIGNAL(balanceChanged(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)),
                   this, SLOT(balanceChanged(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)));
        disconnect(walletModel->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(displayUnitChanged(int)));
    }
}
