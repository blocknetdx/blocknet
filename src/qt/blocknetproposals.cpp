// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetproposals.h"
#include "blocknetvars.h"
#include "blockneticonaltbtn.h"
#include "blocknetdropdown.h"
#include "blocknetformbtn.h"
#include "blocknethdiv.h"

#include "servicenode-budget.h"
#include "activeservicenode.h"
#include "servicenodeconfig.h"
#include "rpcserver.h"
#include "optionsmodel.h"
#include "bitcoinunits.h"
#include "guiutil.h"
#include "uint256.h"
#include "key.h"

#include <QVariant>
#include <QHeaderView>
#include <QDebug>
#include <QFormLayout>
#include <QAbstractItemView>
#include <QMessageBox>
#include <QRadioButton>
#include <QCheckBox>
#include <QApplication>
#include <QClipboard>

BlocknetProposals::BlocknetProposals(QFrame *parent) : QFrame(parent), layout(new QVBoxLayout),
                                                       walletModel(nullptr), contextMenu(new QMenu) {
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->setContentsMargins(46, 10, 50, 0);
    this->setLayout(layout);

    titleLbl = new QLabel(tr("Proposals"));
    titleLbl->setObjectName("h4");
    titleLbl->setFixedHeight(26);

    auto *topBox = new QFrame;
    auto *topBoxLayout = new QHBoxLayout;
    topBoxLayout->setContentsMargins(QMargins());
    topBox->setLayout(topBoxLayout);

    auto *createProposal = new BlocknetIconAltBtn(":/redesign/QuickActions/ProposalsIcon.png");

    buttonLbl = new QLabel(tr("Create New Proposal"));
    buttonLbl->setObjectName("h4");

    filterLbl = new QLabel(tr("Filter by:"));
    filterLbl->setObjectName("title");

    QStringList list{tr("All Proposals"), tr("Active"), tr("Upcoming"), tr("Completed")};
    proposalsDropdown = new BlocknetDropdown(list);

//    topBoxLayout->addWidget(createProposal, 0, Qt::AlignLeft);
//    topBoxLayout->addWidget(buttonLbl, 0, Qt::AlignLeft);
    topBoxLayout->addStretch(1);
    topBoxLayout->addWidget(filterLbl);
    topBoxLayout->addWidget(proposalsDropdown);

    table = new QTableWidget;
    table->setContentsMargins(QMargins());
    table->setColumnCount(COLUMN_PADDING2 + 1);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setFocusPolicy(Qt::NoFocus);
    table->setAlternatingRowColors(true);
    table->setColumnWidth(COLUMN_COLOR, 3);
    table->setColumnWidth(COLUMN_VOTE, 150);
    table->setShowGrid(false);
    table->setFocusPolicy(Qt::NoFocus);
    table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->setContextMenuPolicy(Qt::CustomContextMenu);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->verticalHeader()->setDefaultSectionSize(78);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    table->horizontalHeader()->setSortIndicatorShown(true);
    table->horizontalHeader()->setSectionsClickable(true);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_NAME, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_COLOR, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_VOTE, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_PAYMENT, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_AMOUNT, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_URL, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_RESULTS, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_PADDING1, QHeaderView::Fixed); table->setColumnWidth(COLUMN_PADDING1, 12);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_PADDING2, QHeaderView::Fixed); table->setColumnWidth(COLUMN_PADDING2, 12);
    table->setColumnHidden(COLUMN_HASH, true);
    table->setHorizontalHeaderLabels({ "", "", "", tr("Name"), tr("Superblock"), tr("Amount"), tr("Payments"), tr("Link"), tr("Status"), tr("Results"), "", "" });

    layout->addWidget(titleLbl);
    layout->addSpacing(15);
    layout->addWidget(topBox);
    layout->addSpacing(15);
    layout->addWidget(table);
    layout->addSpacing(20);

    // context menu
    auto *copyName = new QAction(tr("Copy Proposal Name"), this);
    auto *copyUrl = new QAction(tr("Copy Proposal URL"), this);
    auto *copyHash = new QAction(tr("Copy Proposal Hash"), this);
    auto *copyYes = new QAction(tr("Copy Yes Vote Command"), this);
    auto *copyNo = new QAction(tr("Copy No Vote Command"), this);
    auto *copyYesMany = new QAction(tr("Copy Yes Vote-Many Command"), this);
    auto *copyNoMany = new QAction(tr("Copy No Vote-Many Command"), this);
    contextMenu->addAction(copyName);
    contextMenu->addAction(copyUrl);
    contextMenu->addAction(copyHash);
    contextMenu->addSeparator();
    contextMenu->addAction(copyYes);
    contextMenu->addAction(copyNo);
    contextMenu->addSeparator();
    contextMenu->addAction(copyYesMany);
    contextMenu->addAction(copyNoMany);

    // Timer used to check for vote capabilities and refresh proposals
    int timerInterval = 5000;
    timer = new QTimer(this);
    timer->setInterval(timerInterval);
    connect(timer, &QTimer::timeout, this, [this]() {
        bool cannotVote = !canVote();
        if (cannotVote) table->setColumnWidth(COLUMN_VOTE, 0);
        else table->setColumnWidth(COLUMN_VOTE, 150);
        table->setColumnHidden(COLUMN_VOTE, cannotVote);

        // should we force refresh?
        bool notSynced = !servicenodeSync.IsSynced();
        bool forceRefresh = syncInProgress != notSynced;
        syncInProgress = notSynced;
        refresh(forceRefresh);
    });
    timer->start(timerInterval);

    //connect(createproposal, SIGNAL(clicked()), this, SLOT(onCreateProposal()));
    connect(table, &QTableWidget::itemSelectionChanged, this, [this]() {
        lastSelection = QDateTime::currentMSecsSinceEpoch();
    });
    connect(table, &QTableWidget::itemClicked, this, [this](QTableWidgetItem *item) {
        if (item && item->row() == lastRow && (QDateTime::currentMSecsSinceEpoch() - lastSelection > 250)) // defeat event loop w/ time check
            table->clearSelection();
        auto items = table->selectedItems();
        if (items.count() > 0)
            lastRow = items[0]->row();
        else lastRow = -1;
    });
    connect(table, &QTableWidget::customContextMenuRequested, this, &BlocknetProposals::showContextMenu);
    connect(proposalsDropdown, &BlocknetDropdown::valueChanged, this, &BlocknetProposals::onFilter);

    connect(copyName, &QAction::triggered, this, [this]() {
        if (contextItem == nullptr)
            return;
        auto *nameItem = table->item(contextItem->row(), COLUMN_NAME);
        if (nameItem)
            QApplication::clipboard()->setText(nameItem->data(Qt::DisplayRole).toString(), QClipboard::Clipboard);
    });
    connect(copyUrl, &QAction::triggered, this, [this]() {
        if (contextItem == nullptr)
            return;
        auto *urlItem = table->item(contextItem->row(), COLUMN_URL);
        if (urlItem)
            QApplication::clipboard()->setText(urlItem->data(Qt::DisplayRole).toString(), QClipboard::Clipboard);
    });
    connect(copyHash, &QAction::triggered, this, [this]() {
        if (contextItem == nullptr)
            return;
        auto *hashItem = table->item(contextItem->row(), COLUMN_HASH);
        if (hashItem) {
            auto propHash = hashItem->data(Qt::DisplayRole).toString();
            QApplication::clipboard()->setText(propHash, QClipboard::Clipboard);
        }
    });
    connect(copyYes, &QAction::triggered, this, [this]() {
        if (contextItem == nullptr)
            return;
        auto *hashItem = table->item(contextItem->row(), COLUMN_HASH);
        if (hashItem) {
            auto propHash = hashItem->data(Qt::DisplayRole).toString();
            QApplication::clipboard()->setText(QString("mnbudget vote %1 yes").arg(propHash), QClipboard::Clipboard);
        }
    });
    connect(copyNo, &QAction::triggered, this, [this]() {
        if (contextItem == nullptr)
            return;
        auto *hashItem = table->item(contextItem->row(), COLUMN_HASH);
        if (hashItem) {
            auto propHash = hashItem->data(Qt::DisplayRole).toString();
            QApplication::clipboard()->setText(QString("mnbudget vote %1 no").arg(propHash), QClipboard::Clipboard);
        }
    });
    connect(copyYesMany, &QAction::triggered, this, [this]() {
        if (contextItem == nullptr)
            return;
        auto *hashItem = table->item(contextItem->row(), COLUMN_HASH);
        if (hashItem) {
            auto propHash = hashItem->data(Qt::DisplayRole).toString();
            QApplication::clipboard()->setText(QString("mnbudget vote-many %1 yes").arg(propHash), QClipboard::Clipboard);
        }
    });
    connect(copyNoMany, &QAction::triggered, this, [this]() {
        if (contextItem == nullptr)
            return;
        auto *hashItem = table->item(contextItem->row(), COLUMN_HASH);
        if (hashItem) {
            auto propHash = hashItem->data(Qt::DisplayRole).toString();
            QApplication::clipboard()->setText(QString("mnbudget vote-many %1 no").arg(propHash), QClipboard::Clipboard);
        }
    });

    syncInProgress = !servicenodeSync.IsSynced();
}

void BlocknetProposals::initialize() {
    if (!walletModel)
        return;

    dataModel.clear();
    auto current_block = budget.GetChainHeight();
    auto passingProposals = budget.GetBudget();
    auto finalizedBudgets = budget.GetFinalizedBudgets();

    for (CBudgetProposal *proposal : budget.GetAllProposals()) {
        string budgetHash = proposal->GetHash().ToString();
        int nBlockHeight = proposal->GetBlockStart();
        bool budgetWasFound = false;
        bool finalizedBudgetIsValid = false;
        bool proposalInBudget = false;

        for (CFinalizedBudget *finalizedBudget : finalizedBudgets) {
            std::vector<CTxBudgetPayment> payments;
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

        for (CBudgetProposal *budgetProposal : passingProposals) {
            if (budgetProposal->GetHash() == proposal->GetHash()) {
                proposalInBudget = true;
                break;
            }
        }

        int userVoteInt = -1;
        auto userVote = QString();
        for (auto &vote : proposal->mapVotes) {
            if (proposal->GetHash() == vote.second.nProposalHash && hasVote(vote.second.vin)) {
                userVote = QString::fromStdString(vote.second.GetVoteString());
                userVoteInt = vote.second.nVote;
                break;
            }
        }

        QString status = tr("Voting");
        QString results = tr("Failing");
        statusflags statusColor = STATUS_REJECTED;

        if (current_block >= proposal->GetBlockStart() && current_block <= proposal->GetBlockEndCycle() && (budgetWasFound || proposal->IsPassing())) {
            results = tr("Passed");
            status = tr("Finished");
            statusColor = STATUS_PASSED;
        }
        else if (current_block >= proposal->GetBlockStart() && current_block <= proposal->GetBlockEndCycle() && proposal->GetRemainingPaymentCount() > 0 && (proposalInBudget || proposal->IsPassing())) {
            results = tr("Passing");
            statusColor = STATUS_PASSED;
        }
        else if (current_block >= proposal->GetBlockStart() && current_block <= proposal->GetBlockEndCycle() && proposal->GetRemainingPaymentCount() > 0) {
            results = tr("Failing");
            statusColor = STATUS_REJECTED;
        }
        else if (current_block >= proposal->GetBlockEndCycle() && proposal->IsPassing()) {
            results = tr("Passed");
            status = tr("Finished");
            statusColor = STATUS_PASSED;
        }
        else if (current_block >= proposal->GetBlockStart()) {
            results = tr("Failed");
            status = tr("Finished");
            statusColor = STATUS_REJECTED;
        }
        else if (current_block < proposal->GetBlockStart() && (proposalInBudget || proposal->IsPassing())) {
            results = tr("Passing");
            statusColor = STATUS_PASSED;
        }
        else if (current_block < proposal->GetBlockStart() && proposal->GetBlockStart() != budget.NextBudgetBlock() && proposal->IsPassing()) {
            results = tr("Pending");
            statusColor = STATUS_PASSED;
        }
        else if (current_block < proposal->GetBlockStart() && proposal->GetBlockStart() != budget.NextBudgetBlock()) {
            results = tr("Pending");
            statusColor = STATUS_IN_PROGRESS;
        }

        // If proposal is invalid
        std::string perr = "";
        auto valid = proposal->IsValid(perr);
        if (!valid && perr != "Invalid nBlockEnd (end too early)") {
            status = tr("Invalid");
            statusColor = STATUS_REJECTED;
        }

        Proposal proposalData = {
            proposal->GetHash(),
            statusColor,
            QString::fromStdString(proposal->GetName()),
            proposal->GetBlockStart(),
            proposal->GetBlockEndCycle(),
            proposal->GetAmount(),
            (int64_t)proposal->GetTotalPaymentCount(),
            QString::fromStdString(proposal->GetURL()),
            status,
            results,
            userVoteInt,
            userVote
        };
        dataModel << proposalData;
    }

    // Sort on superblock descending
    std::sort(dataModel.begin(), dataModel.end(), [](const Proposal &a, const Proposal &b) {
        return a.superblock > b.superblock;
    });

    this->setData(dataModel);
}

void BlocknetProposals::setWalletModel(WalletModel *w) {
    if (walletModel == w)
        return;

    walletModel = w;
    if (!walletModel || !walletModel->getOptionsModel())
        return;

    initialize();
}

void BlocknetProposals::setData(QVector<Proposal> data) {
    this->filteredData = data;

    unwatch();
    table->clearContents();
    table->setRowCount(this->filteredData.count());
    table->setSortingEnabled(false);

    for (int i = 0; i < this->filteredData.count(); ++i) {
        auto &d = this->filteredData[i];

        // color indicator
        auto *colorItem = new QTableWidgetItem;
        auto *indicatorBox = new QFrame;
        indicatorBox->setObjectName("indicator");
        indicatorBox->setContentsMargins(QMargins());
        indicatorBox->setProperty("state", QVariant(d.color));
        indicatorBox->setFixedWidth(3);
        table->setCellWidget(i, COLUMN_COLOR, indicatorBox);
        table->setItem(i, COLUMN_COLOR, colorItem);

        // proposal hash
        auto *hashItem = new QTableWidgetItem;
        hashItem->setData(Qt::DisplayRole, QString::fromStdString(d.hash.GetHex()));
        table->setItem(i, COLUMN_HASH, hashItem);

        // padding 1
        auto *pad1Item = new QTableWidgetItem;
        table->setItem(i, COLUMN_PADDING1, pad1Item);

        // name
        auto *nameItem = new QTableWidgetItem;
        nameItem->setData(Qt::DisplayRole, d.name);
        table->setItem(i, COLUMN_NAME, nameItem);

        // superblock
        auto *blockItem = new QTableWidgetItem;
        blockItem->setData(Qt::DisplayRole, d.superblock);
        table->setItem(i, COLUMN_SUPERBLOCK, blockItem);

        // amount
        auto *amountItem = new BlocknetProposals::NumberItem;
        amountItem->amount = d.amount;
        amountItem->setData(Qt::DisplayRole, BitcoinUnits::floorWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), d.amount));
        table->setItem(i, COLUMN_AMOUNT, amountItem);

        // payments
        auto *paymentsItem = new BlocknetProposals::NumberItem;
        paymentsItem->amount = d.payments;
        paymentsItem->setData(Qt::DisplayRole, QString("     %1").arg(QString::number(d.payments)));
        paymentsItem->setData(Qt::DisplayRole, QString("     %1").arg(QString::number(d.payments)));
        table->setItem(i, COLUMN_PAYMENT, paymentsItem);

        // url
        auto *urlItem = new QTableWidgetItem;
        urlItem->setData(Qt::DisplayRole, d.url);
        table->setItem(i, COLUMN_URL, urlItem);

        // status
        auto *statusItem = new QTableWidgetItem;
        statusItem->setData(Qt::DisplayRole, d.status);
        table->setItem(i, COLUMN_STATUS, statusItem);

        // results
        auto *resultsItem = new QTableWidgetItem;
        resultsItem->setData(Qt::DisplayRole, d.results);
        table->setItem(i, COLUMN_RESULTS, resultsItem);

        // voted
        auto *votedItem = new QTableWidgetItem;
        auto *widget = new QWidget();
        widget->setContentsMargins(QMargins());
        widget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
        auto *boxLayout = new QVBoxLayout;
        boxLayout->setContentsMargins(QMargins());
        boxLayout->setSpacing(0);
        widget->setLayout(boxLayout);

        auto voteText = QString();
        switch (d.vote) {
            case VOTE_YES:
            case VOTE_NO:
            case VOTE_ABSTAIN:
                if (!d.voteString.isEmpty())
                    voteText = QString("%1 %2").arg(tr("Voted"), d.voteString);
                break;
            default:
                break;
        }

        // If we've already voted, display a label with vote status above the vote button
        auto *voteLbl = new QLabel;
        voteLbl->setObjectName("h6");
        if (!voteText.isEmpty()) {
            voteLbl->setText(voteText);
            boxLayout->addWidget(voteLbl, 0, Qt::AlignCenter);
        } else if (budget.GetChainHeight() >= d.superblock) {
            voteLbl->setText(tr("Did not vote"));
            boxLayout->addWidget(voteLbl, 0, Qt::AlignCenter);
        }

        // Only show vote button if proposal voting is in progress
        if (budget.GetChainHeight() < d.superblock) {
            auto *button = new BlocknetFormBtn;
            button->setText(voteText.isEmpty() ? tr("Vote") : tr("Change Vote"));
            button->setFixedSize(100, 40);
            button->setID(QString::fromStdString(d.hash.GetHex()));
            boxLayout->addWidget(button, 0, Qt::AlignCenter);
            boxLayout->addSpacing(6);
            connect(button, &BlocknetFormBtn::clicked, this, &BlocknetProposals::onVote);
        }

        table->setCellWidget(i, COLUMN_VOTE, widget);
        table->setItem(i, COLUMN_VOTE, votedItem);

        // padding 2
        auto *pad2Item = new QTableWidgetItem;
        table->setItem(i, COLUMN_PADDING2, pad2Item);
    }
    // Hide the vote column if we're not able to vote
    bool cannotVote = !canVote();
    if (cannotVote) table->setColumnWidth(COLUMN_VOTE, 0);
    else table->setColumnWidth(COLUMN_VOTE, 150);
    table->setColumnHidden(COLUMN_VOTE, cannotVote);
    table->setSortingEnabled(true);
    watch();
}

QVector<BlocknetProposals::Proposal> BlocknetProposals::filtered(int filter, int chainHeight) {
    QVector<Proposal> r;
    for (auto &d : dataModel) {
        switch (filter) {
            case FILTER_ACTIVE: {
                if (chainHeight >= d.superblock && chainHeight < d.endblock)
                    r.push_back(d);
                break;
            }
            case FILTER_UPCOMING: {
                if (chainHeight < d.superblock)
                    r.push_back(d);
                break;
            }
            case FILTER_COMPLETED: {
                if (chainHeight >= d.endblock)
                    r.push_back(d);
                break;
            }
            case FILTER_ALL:
            default:
                r.push_back(d);
                break;
        }
    }
    return r;
}

bool BlocknetProposals::canVote() {
    return (servicenodeConfig.getEntries().size() > 0 || activeServicenode.status == ACTIVE_SERVICENODE_STARTED) && servicenodeSync.IsSynced();
}

/**
 * @brief Returns true if our client is a servicenode or a valid activator that matches the specified vin.
 * @param vin
 * @return
 */
bool BlocknetProposals::hasVote(const CTxIn &vin) {
    if (activeServicenode.vin == vin) // if we're a valid servicenode
        return true;

    // If we're not a servicenode check if we're an activator with a valid servicenode vin
    for (auto &mne : servicenodeConfig.getEntries()) {
        CPubKey pubKeyServicenode;
        CKey keyServicenode;
        std::string errorMessage = "";
        if (!obfuScationSigner.SetKey(mne.getPrivKey(), errorMessage, keyServicenode, pubKeyServicenode))
            continue;
        CServicenode *pmn = mnodeman.Find(pubKeyServicenode);
        if (pmn == nullptr)
            continue;
        if (pmn->vin == vin)
            return true;
    }

    return false;
}

/**
 * @brief Refreshes the display if necessary.
 * @param force Set true to force a refresh (bypass all checks).
 */
void BlocknetProposals::refresh(bool force) {
    if (!force && dataModel.size() == static_cast<int>(budget.GetAllProposals().size())) // ignore if the proposal list hasn't changed
        return;
    initialize();
    onFilter();
}

/**
 * @brief Filters the data model based on the current filter dropdown filter flag.
 */
void BlocknetProposals::onFilter() {
    setData(filtered(proposalsDropdown->currentIndex(), budget.GetChainHeight()));
}

void BlocknetProposals::onVote() {
    auto *btn = qobject_cast<BlocknetFormBtn*>(sender());
    auto proposalHash = uint256S(btn->getID().toStdString());
    Proposal foundProposal;

    bool found = false;
    for (auto &proposal : dataModel) {
        if (proposal.hash == proposalHash) {
            found = true;
            foundProposal = proposal;
            break;
        }
    }
    if (!found) { // do nothing if no proposal found
        QMessageBox::warning(this, tr("No proposal found"), tr("Unable to vote on this proposal, please try again later."));
        return;
    }

    auto *dialog = new BlocknetProposalsVoteDialog(foundProposal, walletModel->getOptionsModel()->getDisplayUnit());
    dialog->setStyleSheet(GUIUtil::loadStyleSheet());
    connect(dialog, &BlocknetProposalsVoteDialog::submitVote, this, [this, dialog](uint256 propHash, bool yes, bool no, bool abstain, bool voteMany) {
        int voteIn = (yes ? VOTE_YES : (no ? VOTE_NO : VOTE_ABSTAIN));
        QString errorMsg = QString();

        if (voteMany) { // Vote from activation wallet
            auto &nodeEntries = servicenodeConfig.getEntries();
            if (nodeEntries.size() > 0) {
                uint successfulVotes = 0;
                for (const auto &mne : nodeEntries) {
                    std::vector<unsigned char> vchServiceNodeSignature;
                    std::string strServiceNodeSignMessage;

                    CKey keyCollateralAddress;
                    CPubKey pubKeyServicenode;
                    CKey keyServicenode;

                    std::string errorMessage;
                    if (!obfuScationSigner.SetKey(mne.getPrivKey(), errorMessage, keyServicenode, pubKeyServicenode)) {
                        errorMsg.append(tr("Couldn't prove you own this Servicenode %1: %2\n").arg(QString::fromStdString(mne.getAlias()), QString::fromStdString(errorMessage)));
                        continue;
                    }

                    CServicenode *pmn = mnodeman.Find(pubKeyServicenode);
                    if (pmn == nullptr) {
                        errorMsg.append(tr("Can't find your Servicenode in the list: %1\n").arg(QString::fromStdString(mne.getAlias())));
                        continue;
                    }

                    CBudgetVote vote(pmn->vin, propHash, voteIn);
                    if (!vote.Sign(keyServicenode, pubKeyServicenode)) {
                        errorMsg.append(tr("Failed to sign the vote %1\n").arg(QString::fromStdString(mne.getAlias())));
                        continue;
                    }

                    std::string strError = "";
                    if (budget.UpdateProposal(vote, nullptr, strError)) {
                        budget.mapSeenServicenodeBudgetVotes.insert(make_pair(vote.GetHash(), vote));
                        vote.Relay();
                        ++successfulVotes;
                    } else {
                        errorMsg.append(QString("Error submitting your vote to the network %1: %2\n").arg(QString::fromStdString(mne.getAlias()), QString::fromStdString(strError)));
                    }
                }

                // If there's errors, indicate that there were successful votes
                if (successfulVotes > 0 && !errorMsg.isEmpty())
                    errorMsg.append(tr("Even though there were vote errors %1 votes were submitted successfully by other servicenodes.").arg(successfulVotes));

            } else { // Indicate error, no entries found
                errorMsg.append(tr("Couldn't submit your vote. \"Vote Many\" requires Servicenode entries to be specified in your servicenode.conf"));
            }
        } else { // Vote from servicenode
            bool success = [&]() -> bool {
                CPubKey pubKeyServicenode;
                CKey keyServicenode;

                std::string errorMessage;
                if (!obfuScationSigner.SetKey(strServiceNodePrivKey, errorMessage, keyServicenode, pubKeyServicenode)) {
                    errorMsg.append(tr("Servicenode check failed. If this is not a Servicenode, try \"Vote many\": %1\n").arg(QString::fromStdString(errorMessage)));
                    return false;
                }

                CServicenode *pmn = mnodeman.Find(activeServicenode.vin);
                if (pmn == nullptr) {
                    errorMsg.append(tr("Can't find your Servicenode in the list: %1\n").arg(QString::fromStdString(activeServicenode.GetStatus())));
                    return false;
                }

                CBudgetVote vote(activeServicenode.vin, propHash, voteIn);
                if (!vote.Sign(keyServicenode, pubKeyServicenode)) {
                    errorMsg.append(tr("Failed to sign the vote: %1\n").arg(QString::fromStdString(activeServicenode.GetStatus())));
                    return false;
                }

                std::string strError = "";
                if (budget.UpdateProposal(vote, nullptr, strError)) {
                    budget.mapSeenServicenodeBudgetVotes.insert(make_pair(vote.GetHash(), vote));
                    vote.Relay();
                    return true;
                } else {
                    errorMsg.append(QString("Error submitting your vote to the network: %1\n").arg(QString::fromStdString(strError)));
                    return false;
                }
            }();
        }

        if (!errorMsg.isEmpty())
            QMessageBox::warning(this, tr("Vote Submission Issue"), errorMsg);
        else {// close dialog if no errors
            dialog->close();
            // refresh data
            initialize();
            onFilter();
        }
    });
    dialog->exec();
}

void BlocknetProposals::onItemChanged(QTableWidgetItem *item) {
    if (dataModel.count() > item->row()) {
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

void BlocknetProposals::showContextMenu(QPoint pt) {
    auto *item = table->itemAt(pt);
    if (!item) {
        contextItem = nullptr;
        return;
    }
    contextItem = item;
    contextMenu->exec(QCursor::pos());
}

/**
 * @brief Dialog that manages the vote submission for the specified proposal.
 * @param proposal Proposal that's being voting on
 * @param parent Optional parent widget to attach to
 */
BlocknetProposalsVoteDialog::BlocknetProposalsVoteDialog(BlocknetProposals::Proposal &proposal, int displayUnit, QWidget *parent) : QDialog(parent) {
    auto *layout = new QVBoxLayout;
    layout->setContentsMargins(30, 10, 30, 10);
    this->setLayout(layout);
    this->setMinimumWidth(550);

    // Find the stored proposal
    QString url = proposal.url;
    QString amount = BitcoinUnits::floorWithUnit(displayUnit, proposal.amount);
    QString paymentCount = QString::number(proposal.payments);

    auto *titleLbl = new QLabel(tr("Vote on %1").arg(proposal.name));
    titleLbl->setObjectName("h2");
    titleLbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    auto *descBox = new QFrame;
    descBox->setContentsMargins(QMargins());
    descBox->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);
    auto *descBoxLayout = new QGridLayout;
    descBox->setLayout(descBoxLayout);
    auto *urlLbl = new QLabel(tr("Link"));                urlLbl->setObjectName("description");
    auto *urlVal = new QLabel(url);                       urlVal->setObjectName("value"); urlVal->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto paymentsStr = QString::number(proposal.payments);
    auto *paymentCountLbl = new QLabel(tr("Payments"));   paymentCountLbl->setObjectName("description");
    auto *paymentCountVal = new QLabel(paymentsStr);      paymentCountVal->setObjectName("value");
    auto amountStr = BitcoinUnits::floorWithUnit(displayUnit, proposal.amount);
    auto *amountLbl = new QLabel(tr("Amount"));           amountLbl->setObjectName("description");
    auto *amountVal = new QLabel(amountStr);              amountVal->setObjectName("value");
    auto summaryStr = BitcoinUnits::floorWithUnit(displayUnit, proposal.payments * proposal.amount);
    auto *summaryLbl = new QLabel(tr("Total w/ Payments")); summaryLbl->setObjectName("description");
    auto *summaryVal = new QLabel(summaryStr);            summaryVal->setObjectName("value");
    auto *pl = new QLabel;
    descBoxLayout->addWidget(urlLbl, 0, 0);           descBoxLayout->addWidget(urlVal, 0, 1);          descBoxLayout->addWidget(pl, 0, 2); descBoxLayout->setRowMinimumHeight(0, 22);
    descBoxLayout->addWidget(paymentCountLbl, 1, 0);  descBoxLayout->addWidget(paymentCountVal, 1, 1); descBoxLayout->addWidget(pl, 1, 2); descBoxLayout->setRowMinimumHeight(1, 22);
    descBoxLayout->addWidget(amountLbl, 2, 0);        descBoxLayout->addWidget(amountVal, 2, 1);       descBoxLayout->addWidget(pl, 2, 2); descBoxLayout->setRowMinimumHeight(2, 22);
    if (proposal.payments > 1) {
        descBoxLayout->addWidget(summaryLbl, 3, 0);   descBoxLayout->addWidget(summaryVal, 3, 1);      descBoxLayout->addWidget(pl, 3, 2); descBoxLayout->setRowMinimumHeight(3, 22);
    }
    descBoxLayout->setColumnStretch(2, 1);

    auto *div1 = new BlocknetHDiv;
    auto *div2 = new BlocknetHDiv;
    auto *div3 = new BlocknetHDiv;

    auto *choiceBox = new QFrame;
    choiceBox->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);
    auto *choiceBoxLayout = new QHBoxLayout;
    choiceBoxLayout->setContentsMargins(QMargins());
    choiceBoxLayout->setSpacing(20);
    choiceBox->setLayout(choiceBoxLayout);
    auto *approveLbl = new QLabel(tr("Do you approve?"));
    approveLbl->setObjectName("h5");
    auto *yesRb = new QRadioButton(tr("Yes"));
    auto *noRb = new QRadioButton(tr("No"));
    auto *abstainRb = new QRadioButton(tr("Abstain"));
    choiceBoxLayout->addWidget(yesRb);
    choiceBoxLayout->addWidget(noRb);
    choiceBoxLayout->addWidget(abstainRb);

    auto *voteAllBox = new QFrame;
    voteAllBox->setContentsMargins(QMargins());
    auto *voteAllBoxLayout = new QVBoxLayout;
    voteAllBoxLayout->setContentsMargins(QMargins());
    voteAllBoxLayout->setSpacing(10);
    voteAllBox->setLayout(voteAllBoxLayout);
    auto *voteAllLbl = new QLabel(tr("Do you want to vote with all your Service Nodes?"));
    voteAllLbl->setObjectName("h5");
    auto *voteAllDescLbl = new QLabel(tr("If this is not a Service Node select \"Vote many\" below to vote remotely"));
    voteAllDescLbl->setObjectName("description");
    auto *voteManyCb = new QCheckBox(tr("Vote many"));
    voteManyCb->setToolTip(tr("Vote with all the Service Nodes listed in your servicenode.conf"));
    voteManyCb->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    voteAllBoxLayout->addWidget(voteAllLbl);
    voteAllBoxLayout->addWidget(voteAllDescLbl);
    voteAllBoxLayout->addSpacing(10);
    voteAllBoxLayout->addWidget(voteManyCb);

    auto *btnBox = new QFrame;
    btnBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
    auto *btnBoxLayout = new QHBoxLayout;
    btnBoxLayout->setContentsMargins(QMargins());
    btnBoxLayout->setSpacing(15);
    btnBox->setLayout(btnBoxLayout);
    auto *voteBtn = new BlocknetFormBtn;
    voteBtn->setText(tr("Vote"));
    auto *cancelBtn = new BlocknetFormBtn;
    cancelBtn->setObjectName("cancel");
    cancelBtn->setText(tr("Cancel"));
    btnBoxLayout->addWidget(cancelBtn, 0, Qt::AlignRight | Qt::AlignBottom);
    btnBoxLayout->addWidget(voteBtn, 0, Qt::AlignLeft | Qt::AlignBottom);

    layout->addSpacing(20);
    layout->addWidget(titleLbl);
    layout->addSpacing(10);
    layout->addWidget(descBox);
    layout->addSpacing(10);
    layout->addWidget(div1);
    layout->addSpacing(10);
    layout->addWidget(approveLbl);
    layout->addSpacing(10);
    layout->addWidget(choiceBox);
    layout->addSpacing(10);
    layout->addWidget(div2);
    layout->addSpacing(10);
    layout->addWidget(voteAllBox);
    layout->addSpacing(10);
    layout->addWidget(div3);
    layout->addSpacing(30);
    layout->addStretch(1);
    layout->addWidget(btnBox);
    layout->addSpacing(30);

    connect(voteBtn, &BlocknetFormBtn::clicked, this, [this, proposal, yesRb, noRb, abstainRb, voteManyCb]() {
        if (!yesRb->isChecked() && !noRb->isChecked() && !abstainRb->isChecked()) {
            QMessageBox::critical(this, tr("Failed to vote"), tr("Please select a vote option (Yes, No, Abstain)"));
            return;
        }
        emit submitVote(proposal.hash, yesRb->isChecked(), noRb->isChecked(), abstainRb->isChecked(), voteManyCb->isChecked());
    });
    connect(cancelBtn, &BlocknetFormBtn::clicked, this, [this]() {
        close();
    });
}
