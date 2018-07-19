// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetproposals.h"
#include "blocknetvars.h"
#include "blockneticonaltbtn.h"
//#include "blocknetdropdown.h"
#include "blocknetformbtn.h"
#include "servicenode-budget.h"
#include "rpcserver.h"
#include "optionsmodel.h"
#include "bitcoinunits.h"
#include "activeservicenode.h"

#include <QVariant>
#include <QHeaderView>
#include <QDebug>
#include <QFormLayout>

BlocknetProposals::BlocknetProposals(QFrame *parent) : QFrame(parent), layout(new QVBoxLayout),
                                                       walletModel(nullptr) {
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->setContentsMargins(46, 10, 50, 0);
    this->setLayout(layout);

    titleLbl = new QLabel(tr("Proposals"));
    titleLbl->setObjectName("h4");
    titleLbl->setFixedHeight(26);

    auto *topGrid = new QFrame;
    //topGrid->setStyleSheet("background-color:rgb(255,255,255)");
    auto *topGridLayout = new QGridLayout;
    topGridLayout->setContentsMargins(QMargins());
    //topGridLayout->setStyleSheet("background-color:red;");
    //topGridLayout->setBackgroundColor(QColor(255, 0, 0, 127));
    topGrid->setLayout(topGridLayout);

    auto *createProposal = new BlocknetIconAltBtn(":/redesign/QuickActions/ProposalsIcon.png");

    buttonLbl = new QLabel(tr("Create New Proposal")); 
    buttonLbl->setObjectName("h4");
    //buttonLbl->setStyleSheet("background-color:yellow;");

    filterLbl = new QLabel(tr("Filter by:"));
    filterLbl->setObjectName("title");

    //auto *proposalsDropdown = new BlocknetDropdown;

    topGridLayout->addWidget(createProposal, 0, 0);
    topGridLayout->addWidget(buttonLbl, 0, 1);
    topGridLayout->addWidget(filterLbl, 0, 2);
    //topGridLayout->addWidget(proposalsDropdown, 0, 3);
    //topGridLayout->addWidget(createProposal, 0, 0, Qt::AlignLeft | Qt::AlignTop);
    //topGridLayout->addWidget(buttonLbl, 0, 1, Qt::AlignLeft);
    //topGridLayout->setColumnStretch(1, 1);

    table = new QTableWidget;
    table->setContentsMargins(QMargins());
    table->setColumnCount(8);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->setColumnWidth(COLUMN_COLOR, 3);
    table->setColumnWidth(COLUMN_VOTE, 150);
    table->setShowGrid(false);
    table->setFocusPolicy(Qt::NoFocus);
    table->setContextMenuPolicy(Qt::CustomContextMenu);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->verticalHeader()->setDefaultSectionSize(60);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    table->horizontalHeader()->setSortIndicatorShown(true);
    table->horizontalHeader()->setSectionsClickable(true);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_COLOR, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_VOTE, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_RESULTS, QHeaderView::ResizeToContents);
    table->setHorizontalHeaderLabels({ "", tr("Name"), tr("SuperBlock #"), tr("Amount"), tr("Payment"), tr("Status"), tr("Results"), "" });

    layout->addWidget(titleLbl);
    layout->addSpacing(20);
    layout->addWidget(topGrid);
    layout->addSpacing(20);
    layout->addWidget(table);

    //connect(createproposal, SIGNAL(clicked()), this, SLOT(onCreateProposal()));
    connect(table->horizontalHeader(), SIGNAL(sectionClicked(int)), this, SLOT(recordSelected()));
}

void BlocknetProposals::initialize() {
    if (!walletModel)
        return;
    
    QVector<UTXO> dataModel;
    auto current_block = chainActive.Height();

    for (CBudgetProposal *proposal : budget.GetAllProposals()) {
        //if (!pbudgetProposal->fValid) continue;
        string strError;
        QString status = "In Progress";
        bool userCanVote = false;
        if (activeServicenode.status == ACTIVE_SERVICENODE_STARTED) userCanVote = true;
        if (current_block >= proposal->GetBlockEnd()) status = "Completed";

        QString results = "";
        statusflags statusColor;
        string budgetHash = proposal->GetHash().ToString();
        int nBlockHeight = proposal->GetBlockStart();
        bool budgetWasFound = false;
        bool finalizedBudgetIsValid = false;
        bool proposalInBudget = false;

        auto budgets = budget.GetFinalizedBudgets();
        for (CFinalizedBudget *finalizedBudget : budget.GetFinalizedBudgets()) {
            std::vector<CTxBudgetPayment> payments;
            std::string ret = "unknown-budget";
            if (finalizedBudget->GetBudgetPayments(nBlockHeight, payments)) {
                for (auto &payment : payments) {
                    if (payment.nProposalHash.ToString() == budgetHash) {
                        budgetWasFound = true;
                        break;
                    }
                }
            }
            if (budgetWasFound) {
                std::string strError = "";
                finalizedBudgetIsValid = finalizedBudget->IsValid(strError, false);
                break;
            }
        }

        for (CBudgetProposal *budgetProposal : budget.GetBudget()) {
            if (budgetProposal->GetHash() == proposal->GetHash()) {
                proposalInBudget = true;
                break;
            }
        }

        bool userVoted = false;
        std::string userVote = "";
        std::map<uint256, CBudgetVote>::iterator it = proposal->mapVotes.begin();
        while (it != proposal->mapVotes.end()) {
            if (proposal->GetHash() == (*it).second.GetHash() && activeServicenode.vin == (*it).second.vin) {
                userVoted = true;
                userVote = (*it).second.GetVoteString();
                break;
            }
            it++;
        }
        voteflags proposalVote = voteflags(0);
        if (userVoted) proposalVote = voteflags(1); 
        else if (userCanVote) proposalVote = voteflags(2);

        if (current_block >= proposal->GetBlockStart() && budgetWasFound && finalizedBudgetIsValid) {
            results = "Passed";
            statusColor = STATUS_PASSED;
        }
        else if (current_block >= proposal->GetBlockStart()) {
            results = "Failed";
            statusColor = STATUS_REJECTED;
        }
        else if (current_block < proposal->GetBlockStart() && proposalInBudget)  {
            results = "Passing";
        }
        else if (current_block < proposal->GetBlockStart()) {
            results = "Failing";
            statusColor = STATUS_IN_PROGRESS;
        }

        UTXO proposalData = { 
            statusColor, 
            QString::fromStdString(proposal->GetName()), 
            proposal->GetBlockStart(), 
            BitcoinUnits::format(walletModel->getOptionsModel()->getDisplayUnit(), proposal->GetAmount()), 
            (int64_t)proposal->GetTotalPaymentCount(), 
            status, 
            results, 
            proposalVote,
            QString::fromStdString(userVote)
        };
        dataModel << proposalData;
    }

    /*UTXO proposal1 = { statusflags(1), "coreteam-p10", 12, 94638638, 4320, "In Progress", "01Y, 03N, 03A", voteflags(0) };
    UTXO proposal2 = { statusflags(0), "coreteam-p02", 9, 74658903, 120, "Completed", "Passed", voteflags(1) };
    UTXO proposal3 = { statusflags(2), "coreteam-p04", 2, 89000764, 235, "Completed", "Rejected", voteflags(2) };
    dataModel << proposal1 << proposal2 << proposal3;*/
    this->setData(dataModel);
}

void BlocknetProposals::setWalletModel(WalletModel *w) {
    if (walletModel == w)
        return;

    walletModel = w;
    if (!walletModel || !walletModel->getOptionsModel())
        return;
}

void BlocknetProposals::setData(QVector<UTXO> dataModel) {
    this->dataModel = dataModel;

    unwatch();
    table->clearContents();
    table->setRowCount(dataModel.count());
    table->setSortingEnabled(false);

    for (int i = 0; i < dataModel.count(); ++i) {
        auto d = dataModel[i];
        QColor backgroundColor = QColor(17, 41, 65);
        if (i%2 == 0) {
            backgroundColor = QColor(23, 47, 73);
        }
        table->setRowHeight(i, 66);

        // color
        auto *colorItem = new QTableWidgetItem;
        switch (d.color) {
            case STATUS_PASSED:
                colorItem->setBackgroundColor(QColor(75,245,198));
                break;
            case STATUS_IN_PROGRESS:
                colorItem->setBackgroundColor(QColor(248,191,28));
                break;
            case STATUS_REJECTED:
                colorItem->setBackgroundColor(QColor(251,127,112));
                break;
            default:
                colorItem->setBackgroundColor(backgroundColor);
        }
        table->setItem(i, COLUMN_COLOR, colorItem);

        // name
        auto *nameItem = new QTableWidgetItem;
        nameItem->setData(Qt::DisplayRole, d.name);
        nameItem->setTextColor(Qt::white);
        nameItem->setFont(QFont("Roboto", 10));
        nameItem->setBackgroundColor(backgroundColor);
        table->setItem(i, COLUMN_NAME, nameItem);

        // superblock
        auto *blockItem = new QTableWidgetItem;
        blockItem->setData(Qt::DisplayRole, d.superblock);
        blockItem->setTextColor(Qt::white);
        blockItem->setFont(QFont("Roboto", 10));
        blockItem->setBackgroundColor(backgroundColor);
        table->setItem(i, COLUMN_SUPERBLOCK, blockItem);

        // amount
        auto *amountItem = new QTableWidgetItem;
        amountItem->setData(Qt::DisplayRole, d.amount);
        amountItem->setTextColor(Qt::white);
        amountItem->setFont(QFont("Roboto", 10));
        amountItem->setBackgroundColor(backgroundColor);
        table->setItem(i, COLUMN_AMOUNT, amountItem);

        // payments
        auto *paymentsItem = new QTableWidgetItem;
        paymentsItem->setData(Qt::DisplayRole, d.payments);
        paymentsItem->setTextColor(Qt::white);
        paymentsItem->setFont(QFont("Roboto", 10));
        paymentsItem->setBackgroundColor(backgroundColor);
        table->setItem(i, COLUMN_PAYMENT, paymentsItem);

        // status
        auto *statusItem = new QTableWidgetItem;
        statusItem->setData(Qt::DisplayRole, d.status);
        statusItem->setTextColor(Qt::white);
        statusItem->setFont(QFont("Roboto", 10));
        statusItem->setBackgroundColor(backgroundColor);
        table->setItem(i, COLUMN_STATUS, statusItem);

        // results
        auto *resultsItem = new QTableWidgetItem;
        resultsItem->setData(Qt::DisplayRole, d.results);
        resultsItem->setTextColor(Qt::white);
        resultsItem->setFont(QFont("Roboto", 10));
        resultsItem->setBackgroundColor(backgroundColor);
        table->setItem(i, COLUMN_RESULTS, resultsItem);

        // voted
        auto *votedItem = new QTableWidgetItem;
        votedItem->setTextAlignment(Qt::AlignCenter);
        switch (d.vote) {
            case VOTE_UNDEFINED:
                break;
            case VOTE_YES:
                votedItem->setData(Qt::DisplayRole, "Voted \"" + d.voteString + "\"");
                break;
            case VOTE_NO: {
                auto *widget = new QWidget();
                auto *button = new BlocknetFormBtn;
                QHBoxLayout *boxLayout = new QHBoxLayout(widget);
                boxLayout->setAlignment(Qt::AlignCenter);
                boxLayout->addWidget(button);
                widget->setLayout(boxLayout);
                button->setText(tr("Vote"));
                button->setFixedSize(100, 40);
                table->setCellWidget(i, COLUMN_VOTE, widget);
                break;
            }
            default:
                break;
        }
        votedItem->setTextColor(Qt::white);
        votedItem->setFont(QFont("Roboto", 10));
        votedItem->setBackgroundColor(backgroundColor);
        table->setItem(i, COLUMN_VOTE, votedItem);
    }

    table->setSortingEnabled(true);
    watch();
}

QVector<BlocknetProposals::UTXO> BlocknetProposals::getData() {
    return dataModel;
}

void BlocknetProposals::onItemChanged(QTableWidgetItem *item) {
    if (dataModel.count() > item->row()) {
        //dataModel[item->row()].voted = item->checkState() == Qt::Checked;
        emit tableUpdated();
    }
}

void BlocknetProposals::unwatch() {
    table->setEnabled(false);
    disconnect(table, &QTableWidget::itemChanged, this, &BlocknetProposals::onItemChanged);
}

void BlocknetProposals::watch() {
    table->setEnabled(true);
    connect(table, &QTableWidget::itemChanged, this, &BlocknetProposals::onItemChanged);
}

void BlocknetProposals::recordSelected() {
    unwatch();
    for (int i=0; i<table->rowCount(); i++) {
        for (int j=0; j<table->columnCount(); j++) {
            QColor backgroundColor = QColor(17, 41, 65);
            if (i%2 == 0) {
                backgroundColor = QColor(23, 47, 73);
            }
            if (j != 0) {
                QTableWidgetItem *item = table->item(i,j);
                item->setBackgroundColor(backgroundColor);
            }
        }
    }
    watch();
}