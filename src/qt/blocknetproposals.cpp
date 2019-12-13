// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetproposals.h>

#include <qt/blocknetcheckbox.h>
#include <qt/blocknetcreateproposal.h>
#include <qt/blocknetdropdown.h>
#include <qt/blocknetguiutil.h>
#include <qt/blocknetformbtn.h>
#include <qt/blocknethdiv.h>
#include <qt/blockneticonbtn.h>
#include <qt/blocknetvars.h>

#include <qt/bitcoinunits.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>

#include <uint256.h>

#include <utility>

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QFormLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QRadioButton>
#include <QVariant>

BlocknetProposals::BlocknetProposals(QFrame *parent) : QFrame(parent), layout(new QVBoxLayout),
                                                       walletModel(nullptr), contextMenu(new QMenu) {
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->setContentsMargins(BGU::spi(46), BGU::spi(10), BGU::spi(50), 0);
    this->setLayout(layout);

    titleLbl = new QLabel(tr("Proposals"));
    titleLbl->setObjectName("h4");
    titleLbl->setFixedHeight(BGU::spi(26));

    auto *topBox = new QFrame;
    auto *topBoxLayout = new QHBoxLayout;
    topBoxLayout->setContentsMargins(QMargins());
    topBox->setLayout(topBoxLayout);

    auto *createProposal = new BlocknetIconBtn(":/redesign/QuickActions/ProposalsIcon.png");

    buttonLbl = new QLabel(tr("Create New Proposal"));
    buttonLbl->setObjectName("h4");

    filterLbl = new QLabel(tr("Filter by:"));
    filterLbl->setObjectName("title");

    QStringList list{tr("All Proposals"), tr("Active"), tr("Upcoming"), tr("Completed")};
    proposalsDropdown = new BlocknetDropdown(list);

    topBoxLayout->addWidget(createProposal, 0, Qt::AlignLeft);
    topBoxLayout->addWidget(buttonLbl, 0, Qt::AlignLeft);
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
    table->setColumnWidth(COLUMN_COLOR, BGU::spi(3));
    table->setColumnWidth(COLUMN_VOTE, BGU::spi(150));
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
    table->horizontalHeader()->setSectionResizeMode(COLUMN_AMOUNT, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_URL, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_DESCRIPTION, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_RESULTS, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_PADDING1, QHeaderView::Fixed); table->setColumnWidth(COLUMN_PADDING1, BGU::spi(12));
    table->horizontalHeader()->setSectionResizeMode(COLUMN_PADDING2, QHeaderView::Fixed); table->setColumnWidth(COLUMN_PADDING2, BGU::spi(12));
    table->setColumnHidden(COLUMN_HASH, true);
    table->setHorizontalHeaderLabels({ "", "", "", tr("Name"), tr("Superblock"), tr("Amount"), tr("Url"), tr("Description"), tr("Status"), tr("Results"), "", "", ""});

    layout->addWidget(titleLbl);
    layout->addSpacing(BGU::spi(15));
    layout->addWidget(topBox);
    layout->addSpacing(BGU::spi(15));
    layout->addWidget(table);
    layout->addSpacing(BGU::spi(20));

    // context menu
    auto *viewDetails = new QAction(tr("View Proposal Details"), this);
    auto *copyName = new QAction(tr("Copy Proposal Name"), this);
    auto *copyUrl = new QAction(tr("Copy Proposal URL"), this);
    auto *copyHash = new QAction(tr("Copy Proposal Hash"), this);
    auto *copyYes = new QAction(tr("Copy Yes Vote Command"), this);
    auto *copyNo = new QAction(tr("Copy No Vote Command"), this);
    contextMenu->addAction(viewDetails);
    contextMenu->addSeparator();
    contextMenu->addAction(copyName);
    contextMenu->addAction(copyUrl);
    contextMenu->addAction(copyHash);
    contextMenu->addSeparator();
    contextMenu->addAction(copyYes);
    contextMenu->addAction(copyNo);

    // Timer used to check for vote capabilities and refresh proposals
    int timerInterval = 5000;
    timer = new QTimer(this);
    timer->setInterval(timerInterval);
    connect(timer, &QTimer::timeout, this, [this]() {
        bool cannotVote = !canVote();
        if (cannotVote) table->setColumnWidth(COLUMN_VOTE, 0);
        else table->setColumnWidth(COLUMN_VOTE, BGU::spi(150));
        table->setColumnHidden(COLUMN_VOTE, cannotVote);
        refresh(false);
    });
    timer->start(timerInterval);

    connect(createProposal, SIGNAL(clicked()), this, SLOT(onCreateProposal()));
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
    connect(table, &QTableWidget::cellDoubleClicked, this, [this](int row, int column) {
        if (row >= filteredData.size())
            return;
        auto data = filteredData[row];
        showProposalDetails(data);
    });
    connect(table, &QTableWidget::customContextMenuRequested, this, &BlocknetProposals::showContextMenu);
    connect(proposalsDropdown, &BlocknetDropdown::valueChanged, this, &BlocknetProposals::onFilter);

    connect(viewDetails, &QAction::triggered, this, [this]() {
        if (contextItem == nullptr || contextItem->row() >= filteredData.size())
            return;
        auto data = filteredData[contextItem->row()];
        showProposalDetails(data);
    });
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
            QApplication::clipboard()->setText(QString("vote %1 yes").arg(propHash), QClipboard::Clipboard);
        }
    });
    connect(copyNo, &QAction::triggered, this, [this]() {
        if (contextItem == nullptr)
            return;
        auto *hashItem = table->item(contextItem->row(), COLUMN_HASH);
        if (hashItem) {
            auto propHash = hashItem->data(Qt::DisplayRole).toString();
            QApplication::clipboard()->setText(QString("vote %1 no").arg(propHash), QClipboard::Clipboard);
        }
    });

    syncInProgress = false;
}

void BlocknetProposals::initialize() {
    if (!walletModel)
        return;
    dataModel.clear();

    const auto currentBlock = getChainHeight();
    const auto nextSuperblock = gov::Governance::nextSuperblock(Params().GetConsensus(), currentBlock);
    auto proposals = gov::Governance::instance().getProposals();
    std::map<int, std::map<gov::Proposal, gov::Tally>> superblockResults;

    // Get all superblock results
    for (const auto & proposal : proposals) {
        const int superblock = proposal.getSuperblock();
        if (superblockResults.count(superblock))
            continue;
        superblockResults[superblock] = gov::Governance::instance()
                .getSuperblockResults(superblock, Params().GetConsensus());
    }
    // Sort proposals descending
    std::sort(proposals.begin(), proposals.end(), [](const gov::Proposal & a, const gov::Proposal & b) {
        return a.getSuperblock() > b.getSuperblock();
    });

    for (const auto & proposal : proposals) {
        const auto & sbResults = superblockResults[proposal.getSuperblock()];

        QString status = tr("Voting");
        QString results = tr("Failing");
        statusflags statusColor = STATUS_REJECTED;

        if (currentBlock >= proposal.getSuperblock()) {
            status = tr("Finished");
            statusColor = STATUS_PASSED;
        }

        if (currentBlock < proposal.getSuperblock() && sbResults.count(proposal)) {
            results = tr("Passing");
            statusColor = STATUS_PASSED;
        }
        else if (currentBlock < proposal.getSuperblock() && !sbResults.count(proposal)) {
            results = tr("Failing");
            statusColor = STATUS_REJECTED;
        }
        else if (currentBlock >= proposal.getSuperblock() && sbResults.count(proposal)) {
            results = tr("Passed");
            status = tr("Finished");
            statusColor = STATUS_PASSED;
        }
        else if (currentBlock >= proposal.getSuperblock() && !sbResults.count(proposal)) {
            results = tr("Failed");
            status = tr("Finished");
            statusColor = STATUS_REJECTED;
        }
        else if (currentBlock < proposal.getSuperblock()) {
            results = tr("Pending");
            statusColor = STATUS_IN_PROGRESS;
        }

        gov::VoteType userVoteInt{gov::ABSTAIN};
        QString userVote;
        CAmount voteAmount{0};

        // If proposal is invalid
        std::string perr;
        if (!proposal.isValid(Params().GetConsensus(), &perr)) {
            status = tr("Invalid");
            statusColor = STATUS_REJECTED;
        } else {
            // how many votes are mine?
            auto wallets = GetWallets();
            const auto castVote = gov::Governance::instance().getMyVotes(proposal.getHash(), pcoinsTip.get(),
                    wallets, Params().GetConsensus());
            const auto votes = std::get<0>(castVote);
            const auto voteType = std::get<1>(castVote);
            const auto voted = std::get<2>(castVote);
            voteAmount = std::get<3>(castVote);
            if (votes > 0) {
                if (votes > 1)
                    userVote = voteType == gov::YES ? tr("%1 YES").arg(QString::number(votes))
                                                    : voteType == gov::ABSTAIN ? tr("%1 ABSTAIN").arg(QString::number(votes))
                                                                               : tr("%1 NO").arg(QString::number(votes));
                else
                    userVote = voteType == gov::YES ? tr("YES")
                                                    : voteType == gov::ABSTAIN ? tr("ABSTAIN")
                                                                               : tr("NO");
            } else if (voted)
                userVote = tr("Insufficient funds");
        }

        BlocknetProposal proposalData = {
            proposal.getHash(),
            statusColor,
            QString::fromStdString(proposal.getName()),
            proposal.getSuperblock(),
            proposal.getAmount(),
            QString::fromStdString(proposal.getUrl()),
            QString::fromStdString(proposal.getDescription()),
            status,
            results,
            userVoteInt,
            userVote,
            voteAmount
        };
        dataModel << proposalData;
    }

    // Sort on superblock descending
    std::sort(dataModel.begin(), dataModel.end(), [](const BlocknetProposal &a, const BlocknetProposal &b) {
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

void BlocknetProposals::setData(QVector<BlocknetProposal> data) {
    this->filteredData = std::move(data);

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
        indicatorBox->setFixedWidth(BGU::spi(3));
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
        amountItem->setData(Qt::DisplayRole, BitcoinUnits::floorWithUnit(walletModel->getOptionsModel()->getDisplayUnit(),
                d.amount, 0, false, BitcoinUnits::separatorStandard));
        table->setItem(i, COLUMN_AMOUNT, amountItem);

        // url
        auto *urlItem = new QTableWidgetItem;
        urlItem->setData(Qt::DisplayRole, d.url);
        table->setItem(i, COLUMN_URL, urlItem);

        // description
        auto *descItem = new QTableWidgetItem;
        descItem->setData(Qt::DisplayRole, d.description);
        table->setItem(i, COLUMN_DESCRIPTION, descItem);

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
            case gov::YES:
            case gov::NO:
            case gov::ABSTAIN:
                if (!d.voteString.isEmpty())
                    voteText = QString("%1 %2").arg(tr("Voted"), d.voteString);
                break;
            default:
                break;
        }

        // If we've already voted, display a label with vote status above the vote button
        auto *voteLbl = new QLabel;
        voteLbl->setObjectName("h6");
        voteLbl->setWordWrap(true);
        voteLbl->setAlignment(Qt::AlignCenter);
        if (!voteText.isEmpty()) {
            voteLbl->setText(voteText);
            boxLayout->addWidget(voteLbl, 0, Qt::AlignCenter);
            table->setRowHeight(i, BGU::spi(50));
        } else if (getChainHeight() >= d.superblock) {
            voteLbl->setText(tr("Did not vote"));
            boxLayout->addWidget(voteLbl, 0, Qt::AlignCenter);
        }

        // Only show vote button if proposal voting is in progress
        if (getChainHeight() <= d.superblock - Params().GetConsensus().proposalCutoff) {
            auto *button = new BlocknetFormBtn;
            button->setText(voteText.isEmpty() ? tr("Vote") : tr("Change Vote"));
            button->setFixedSize(BGU::spi(100), BGU::spi(25));
            button->setID(QString::fromStdString(d.hash.GetHex()));
            boxLayout->addWidget(button, 0, Qt::AlignCenter);
            boxLayout->addSpacing(BGU::spi(3));
            connect(button, &BlocknetFormBtn::clicked, this, &BlocknetProposals::onVote);
            if (voteText.isEmpty())
                table->setRowHeight(i, BGU::spi(50));
            else table->setRowHeight(i, BGU::spi(75));
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
    else table->setColumnWidth(COLUMN_VOTE, BGU::spi(150));
    table->setColumnHidden(COLUMN_VOTE, cannotVote);
    table->setSortingEnabled(true);
    watch();
}

QVector<BlocknetProposals::BlocknetProposal> BlocknetProposals::filtered(int filter, int chainHeight) {
    QVector<BlocknetProposal> r;
    for (auto &d : dataModel) {
        const auto endblock = gov::NextSuperblock(Params().GetConsensus(), d.superblock);
        switch (filter) {
            case FILTER_ACTIVE: {
                if (chainHeight >= d.superblock && chainHeight < endblock)
                    r.push_back(d);
                break;
            }
            case FILTER_UPCOMING: {
                if (chainHeight < d.superblock)
                    r.push_back(d);
                break;
            }
            case FILTER_COMPLETED: {
                if (chainHeight >= endblock)
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
    return walletModel->wallet().getBalance() >= Params().GetConsensus().voteBalance;
}

/**
 * @brief Refreshes the display if necessary.
 * @param force Set true to force a refresh (bypass all checks).
 */
void BlocknetProposals::refresh(bool force) {
    if (!force && dataModel.size() == static_cast<int>(gov::Governance::instance().getProposals().size())) // ignore if the proposal list hasn't changed
        return;
    initialize();
    onFilter();
}

/**
 * @brief Filters the data model based on the current filter dropdown filter flag.
 */
void BlocknetProposals::onFilter() {
    setData(filtered(proposalsDropdown->currentIndex(), getChainHeight()));
}

void BlocknetProposals::onVote() {
    auto *btn = qobject_cast<BlocknetFormBtn*>(sender());
    if (!btn)
        return;
    auto proposalHash = uint256S(btn->getID().toStdString());
    BlocknetProposal foundProposal;

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
    connect(dialog, &BlocknetProposalsVoteDialog::submitVote, this, [this, dialog](uint256 propHash, bool yes, bool no, bool abstain) {
        // Check if proposal is available
        if (!gov::Governance::instance().hasProposal(propHash)) {
            QMessageBox::warning(this, tr("No proposal found"), tr("Unable to find this proposal"));
            return;
        }
        WalletModel::UnlockContext ctx(walletModel->requestUnlock());
        if (!ctx.isValid()) {
            return;
        }
        // Cast votes
        auto proposal = gov::Governance::instance().getProposal(propHash);
        gov::ProposalVote vote(proposal, yes ? gov::VoteType::YES : no ? gov::VoteType::NO : gov::VoteType::ABSTAIN);
        std::string failureReason;
        std::vector<CTransactionRef> txns;
        if (!gov::Governance::instance().submitVotes({vote}, GetWallets(), Params().GetConsensus(), txns, g_connman.get(), &failureReason))
            QMessageBox::warning(this, tr("Vote Submission Issue"), QString::fromStdString(failureReason));
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
        Q_EMIT tableUpdated();
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

void BlocknetProposals::showProposalDetails(const BlocknetProposal & proposal) {
    auto *dialog = new BlocknetProposalsDetailsDialog(proposal, walletModel->getOptionsModel()->getDisplayUnit());
    dialog->setStyleSheet(GUIUtil::loadStyleSheet());
    dialog->exec();
}

/**
 * Dialog that manages the vote submission for the specified proposal.
 * @param proposal Proposal that's being voting on
 * @param parent Optional parent widget to attach to
 */
BlocknetProposalsVoteDialog::BlocknetProposalsVoteDialog(const BlocknetProposals::BlocknetProposal &proposal,
        int displayUnit, QWidget *parent) : QDialog(parent)
{
    auto *layout = new QVBoxLayout;
    layout->setContentsMargins(BGU::spi(30), BGU::spi(10), BGU::spi(30), BGU::spi(10));
    this->setLayout(layout);
    this->setMinimumWidth(BGU::spi(550));
    this->setWindowTitle(tr("Vote on Proposal"));

    // Find the stored proposal
    QString url = proposal.url;
    QString amount = BitcoinUnits::floorWithUnit(displayUnit, proposal.amount);
    QString superblock = QString::number(proposal.superblock);

    auto *titleLbl = new QLabel(tr("Vote on %1").arg(proposal.name));
    titleLbl->setObjectName("h2");
    titleLbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    auto *descBox = new QFrame;
    descBox->setContentsMargins(QMargins());
    descBox->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);
    auto *descBoxLayout = new QGridLayout;
    descBoxLayout->setContentsMargins(QMargins());
    descBox->setLayout(descBoxLayout);
    auto *superblockLbl = new QLabel(tr("Superblock"));   superblockLbl->setObjectName("description");
    auto *superblockVal = new QLabel(superblock);         superblockVal->setObjectName("value"); superblockVal->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *urlLbl = new QLabel(tr("Url"));                urlLbl->setObjectName("description");
    auto *urlVal = new QLabel(url);                       urlVal->setObjectName("value"); urlVal->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto amountStr = BitcoinUnits::floorWithUnit(displayUnit, proposal.amount);
    auto *amountLbl = new QLabel(tr("Amount"));           amountLbl->setObjectName("description");
    auto *amountVal = new QLabel(amountStr);              amountVal->setObjectName("value");
    auto *descLbl = new  QLabel(tr("Description"));       descLbl->setObjectName("description");
    auto *descVal = new QLabel(proposal.description);     descVal->setObjectName("value"); descVal->setWordWrap(true);
    auto *pl = new QLabel;
    descBoxLayout->addWidget(superblockLbl, 0, 0);    descBoxLayout->addWidget(superblockVal, 0, 1);   descBoxLayout->addWidget(pl, 0, 2); descBoxLayout->setRowMinimumHeight(0, 22);
    descBoxLayout->addWidget(urlLbl, 1, 0);           descBoxLayout->addWidget(urlVal, 1, 1);          descBoxLayout->addWidget(pl, 1, 2); descBoxLayout->setRowMinimumHeight(1, 22);
    descBoxLayout->addWidget(amountLbl, 2, 0);        descBoxLayout->addWidget(amountVal, 2, 1);       descBoxLayout->addWidget(pl, 2, 2); descBoxLayout->setRowMinimumHeight(2, 22);
    descBoxLayout->addWidget(descLbl, 3, 0);          descBoxLayout->addWidget(descVal, 3, 1);         descBoxLayout->addWidget(pl, 3, 2); descBoxLayout->setRowMinimumHeight(3, 22);
    descBoxLayout->setColumnStretch(2, 1);

    auto *choiceBox = new QFrame;
    choiceBox->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);
    auto *choiceBoxLayout = new QHBoxLayout;
    choiceBoxLayout->setContentsMargins(QMargins());
    choiceBoxLayout->setSpacing(BGU::spi(20));
    choiceBox->setLayout(choiceBoxLayout);
    auto *approveLbl = new QLabel(tr("Do you approve?"));
    approveLbl->setObjectName("h5");
    auto *yesRb = new QRadioButton(tr("Yes"));
    auto *noRb = new QRadioButton(tr("No"));
    auto *abstainRb = new QRadioButton(tr("Abstain"));
    choiceBoxLayout->addWidget(yesRb);
    choiceBoxLayout->addWidget(noRb);
    choiceBoxLayout->addWidget(abstainRb);

    auto *voteDescBox = new QFrame;
    auto *voteDescBoxLayout = new QVBoxLayout;
    voteDescBox->setContentsMargins(QMargins());
    voteDescBoxLayout->setContentsMargins(QMargins());
    voteDescBox->setLayout(voteDescBoxLayout);
    auto *voteTitleLbl = new QLabel(tr("Network Fee"));
    voteTitleLbl->setObjectName("h5");
    auto *voteDescLbl = new QLabel(tr("Casting a vote will submit a transaction to the network where you'll be charged "
                                      "a minimal network fee in order to secure your vote on-chain."));
    voteDescLbl->setWordWrap(true);
    voteDescLbl->setObjectName("description");
    voteDescBoxLayout->addWidget(voteTitleLbl);
    voteDescBoxLayout->addWidget(voteDescLbl);

    auto *voteInfoBox = new QFrame;
    voteInfoBox->setContentsMargins(QMargins());
    auto *voteInfoBoxLayout = new QGridLayout;
    voteInfoBoxLayout->setContentsMargins(QMargins());
    voteInfoBox->setLayout(voteInfoBoxLayout);
    auto *voteInfoLbl = new QLabel(tr("Your Vote Information"));                                                                   voteInfoLbl->setObjectName("h5");
    auto *voteInfoCoinLbl = new QLabel(tr("Active Vote Amount on this Proposal"));                                                 voteInfoCoinLbl->setObjectName("description");
    auto *voteInfoCoinVal = new QLabel(BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, proposal.voteAmount));                      voteInfoCoinVal->setObjectName("value");
    auto *voteInfoReqLbl = new QLabel(tr("Required Amount per Vote"));                                                             voteInfoReqLbl->setObjectName("description");
    auto *voteInfoReqVal = new QLabel(BitcoinUnits::floorWithUnit(BitcoinUnits::BTC, Params().GetConsensus().voteBalance));        voteInfoReqVal->setObjectName("value");
    auto *voteInfoVotesLbl = new QLabel(tr("Total Votes Counted"));                                                                voteInfoVotesLbl->setObjectName("description");
    auto *voteInfoVotesVal = new QLabel(QString::number(proposal.voteAmount/Params().GetConsensus().voteBalance));                 voteInfoVotesVal->setObjectName("value");
    auto *voteInfoMinLbl = new QLabel(tr("Smallest UTXO Used in Voting"));                                                         voteInfoMinLbl->setObjectName("description");
    auto *voteInfoMinVal = new QLabel(BitcoinUnits::floorWithUnit(BitcoinUnits::BTC, Params().GetConsensus().voteMinUtxoAmount));  voteInfoMinVal->setObjectName("value");
    auto *voteInfoNotesLbl = new QLabel(tr("Reminder* votes are tied to unspent coin, if coin is spent it can cause "
                                           "votes to be invalidated."));                                                           voteInfoNotesLbl->setObjectName("description");
    voteInfoNotesLbl->setWordWrap(true);
    voteInfoBoxLayout->addWidget(voteInfoCoinLbl, 0, 0);
    voteInfoBoxLayout->addWidget(voteInfoCoinVal, 0, 1);
    voteInfoBoxLayout->addWidget(voteInfoReqLbl, 1, 0);
    voteInfoBoxLayout->addWidget(voteInfoReqVal, 1, 1);
    voteInfoBoxLayout->addWidget(voteInfoVotesLbl, 2, 0);
    voteInfoBoxLayout->addWidget(voteInfoVotesVal, 2, 1);
    voteInfoBoxLayout->addWidget(voteInfoMinLbl, 3, 0);
    voteInfoBoxLayout->addWidget(voteInfoMinVal, 3, 1);
    voteInfoBoxLayout->addWidget(new QLabel, 4, 0, 1, 2);
    voteInfoBoxLayout->addWidget(voteInfoNotesLbl, 5, 0, 1, 2);

    auto *btnBox = new QFrame;
    btnBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
    auto *btnBoxLayout = new QHBoxLayout;
    btnBoxLayout->setContentsMargins(QMargins());
    btnBoxLayout->setSpacing(BGU::spi(15));
    btnBox->setLayout(btnBoxLayout);
    auto *voteBtn = new BlocknetFormBtn;
    voteBtn->setText(tr("Vote"));
    auto *cancelBtn = new BlocknetFormBtn;
    cancelBtn->setObjectName("cancel");
    cancelBtn->setText(tr("Cancel"));
    btnBoxLayout->addWidget(cancelBtn, 0, Qt::AlignRight | Qt::AlignBottom);
    btnBoxLayout->addWidget(voteBtn, 0, Qt::AlignLeft | Qt::AlignBottom);

    auto *div1 = new BlocknetHDiv;
    auto *div2 = new BlocknetHDiv;
    auto *div3 = new BlocknetHDiv;
    auto *div4 = new BlocknetHDiv;

    layout->addSpacing(BGU::spi(20));
    layout->addWidget(titleLbl);
    layout->addSpacing(BGU::spi(10));
    layout->addWidget(descBox);
    layout->addSpacing(BGU::spi(10));
    layout->addWidget(div1);
    layout->addSpacing(BGU::spi(10));
    layout->addWidget(approveLbl);
    layout->addSpacing(BGU::spi(10));
    layout->addWidget(choiceBox);
    layout->addSpacing(BGU::spi(10));
    layout->addWidget(div2);
    layout->addSpacing(BGU::spi(10));
    layout->addWidget(voteDescBox);
    layout->addSpacing(BGU::spi(10));
    layout->addWidget(div3);
    layout->addWidget(voteInfoLbl);
    layout->addSpacing(BGU::spi(10));
    layout->addWidget(voteInfoBox);
    layout->addSpacing(BGU::spi(10));
    layout->addWidget(div4);
    layout->addSpacing(BGU::spi(20));
    layout->addStretch(1);
    layout->addWidget(btnBox);
    layout->addSpacing(BGU::spi(10));

    connect(voteBtn, &BlocknetFormBtn::clicked, this, [this, proposal, yesRb, noRb, abstainRb]() {
        if (!yesRb->isChecked() && !noRb->isChecked() && !abstainRb->isChecked()) {
            QMessageBox::critical(this, tr("Failed to vote"), tr("Please select a vote option (Yes, No, Abstain)"));
            return;
        }
        Q_EMIT submitVote(proposal.hash, yesRb->isChecked(), noRb->isChecked(), abstainRb->isChecked());
    });
    connect(cancelBtn, &BlocknetFormBtn::clicked, this, [this]() {
        close();
    });
}

/**
 * Dialog that shows proposal information and user votes.
 * @param proposal Proposal
 * @param displayUnit
 * @param parent Optional parent widget to attach to
 */
BlocknetProposalsDetailsDialog::BlocknetProposalsDetailsDialog(const BlocknetProposals::BlocknetProposal & proposal,
                                                             int displayUnit, QWidget *parent) : QDialog(parent)
{
    auto *layout = new QVBoxLayout;
    layout->setContentsMargins(BGU::spi(30), BGU::spi(10), BGU::spi(30), BGU::spi(10));
    this->setLayout(layout);
    this->setMinimumWidth(BGU::spi(550));
    this->setWindowTitle(tr("Proposal Details"));

    QString url = proposal.url;
    QString amount = BitcoinUnits::floorWithUnit(displayUnit, proposal.amount);
    QString superblock = QString::number(proposal.superblock);

    auto *titleLbl = new QLabel(tr("Proposal %1").arg(proposal.name));
    titleLbl->setObjectName("h2");
    titleLbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    auto *descBox = new QFrame;
    descBox->setContentsMargins(QMargins());
    descBox->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);
    auto *descBoxLayout = new QGridLayout;
    descBoxLayout->setContentsMargins(QMargins());
    descBox->setLayout(descBoxLayout);
    auto *superblockLbl = new QLabel(tr("Superblock"));   superblockLbl->setObjectName("description");
    auto *superblockVal = new QLabel(superblock);         superblockVal->setObjectName("value"); superblockVal->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *urlLbl = new QLabel(tr("Url"));                urlLbl->setObjectName("description");
    auto *urlVal = new QLabel(url);                       urlVal->setObjectName("value"); urlVal->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto amountStr = BitcoinUnits::floorWithUnit(displayUnit, proposal.amount);
    auto *amountLbl = new QLabel(tr("Amount"));           amountLbl->setObjectName("description");
    auto *amountVal = new QLabel(amountStr);              amountVal->setObjectName("value");
    auto *descLbl = new  QLabel(tr("Description"));       descLbl->setObjectName("description");
    auto *descVal = new QLabel(proposal.description);     descVal->setObjectName("value"); descVal->setWordWrap(true);
    auto *pl = new QLabel;
    descBoxLayout->addWidget(superblockLbl, 0, 0);    descBoxLayout->addWidget(superblockVal, 0, 1);   descBoxLayout->addWidget(pl, 0, 2); descBoxLayout->setRowMinimumHeight(0, 22);
    descBoxLayout->addWidget(urlLbl, 1, 0);           descBoxLayout->addWidget(urlVal, 1, 1);          descBoxLayout->addWidget(pl, 1, 2); descBoxLayout->setRowMinimumHeight(1, 22);
    descBoxLayout->addWidget(amountLbl, 2, 0);        descBoxLayout->addWidget(amountVal, 2, 1);       descBoxLayout->addWidget(pl, 2, 2); descBoxLayout->setRowMinimumHeight(2, 22);
    descBoxLayout->addWidget(descLbl, 3, 0);          descBoxLayout->addWidget(descVal, 3, 1);         descBoxLayout->addWidget(pl, 3, 2); descBoxLayout->setRowMinimumHeight(3, 22);
    descBoxLayout->setColumnStretch(2, 1);

    auto *voteInfoBox = new QFrame;
    voteInfoBox->setContentsMargins(QMargins());
    auto *voteInfoBoxLayout = new QGridLayout;
    voteInfoBoxLayout->setContentsMargins(QMargins());
    voteInfoBox->setLayout(voteInfoBoxLayout);
    auto *voteInfoLbl = new QLabel(tr("Your Vote Information"));                                                                    voteInfoLbl->setObjectName("h5");
    auto *voteInfoCoinLbl = new QLabel(tr("Active Vote Amount on this Proposal"));                                                  voteInfoCoinLbl->setObjectName("description");
    auto *voteInfoCoinVal = new QLabel(BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, proposal.voteAmount));                       voteInfoCoinVal->setObjectName("value");
    auto *voteInfoReqLbl = new QLabel(tr("Required Amount per Vote"));                                                              voteInfoReqLbl->setObjectName("description");
    auto *voteInfoReqVal = new QLabel(BitcoinUnits::floorWithUnit(BitcoinUnits::BTC, Params().GetConsensus().voteBalance));         voteInfoReqVal->setObjectName("value");
    auto *voteInfoVotesLbl = new QLabel(tr("Total Votes Counted"));                                                                 voteInfoVotesLbl->setObjectName("description");
    auto *voteInfoVotesVal = new QLabel(QString::number(proposal.voteAmount/Params().GetConsensus().voteBalance));                  voteInfoVotesVal->setObjectName("value");
    auto *voteInfoMinLbl = new QLabel(tr("Smallest UTXO Used in Voting"));                                                          voteInfoMinLbl->setObjectName("description");
    auto *voteInfoMinVal = new QLabel(BitcoinUnits::floorWithUnit(BitcoinUnits::BTC, Params().GetConsensus().voteMinUtxoAmount));   voteInfoMinVal->setObjectName("value");
    voteInfoBoxLayout->addWidget(voteInfoCoinLbl, 0, 0);
    voteInfoBoxLayout->addWidget(voteInfoCoinVal, 0, 1);
    voteInfoBoxLayout->addWidget(voteInfoReqLbl, 1, 0);
    voteInfoBoxLayout->addWidget(voteInfoReqVal, 1, 1);
    voteInfoBoxLayout->addWidget(voteInfoVotesLbl, 2, 0);
    voteInfoBoxLayout->addWidget(voteInfoVotesVal, 2, 1);
    voteInfoBoxLayout->addWidget(voteInfoMinLbl, 3, 0);
    voteInfoBoxLayout->addWidget(voteInfoMinVal, 3, 1);

    auto *voteMsgBox = new QFrame;
    voteMsgBox->setContentsMargins(QMargins());
    auto *voteMsgBoxLayout = new QVBoxLayout;
    voteMsgBoxLayout->setContentsMargins(QMargins());
    voteMsgBox->setLayout(voteMsgBoxLayout);
    auto *voteMsgLbl = new QLabel(tr("Requirements"));
    voteMsgLbl->setObjectName("h5");
    auto *voteMsgNotesLbl = new QLabel(tr("Votes are associated with unspent coin. Spending coin after casting votes may "
                                          "cause those votes to be invalidated. Only p2pkh or p2pk inputs can be used in "
                                          "votes. Casting votes submits transactions to the network and as a result will "
                                          "incur minimal network fees. The ideal way to vote is to put a small 0.01 UTXO "
                                          "in each voting address so that this is used to pay the network fee (instead of "
                                          "larger voting inputs). Any voting inputs used to pay for network fees will not "
                                          "be counted towards a proposal vote."));
    voteMsgNotesLbl->setObjectName("description");
    voteMsgNotesLbl->setWordWrap(true);
    voteMsgBoxLayout->addWidget(voteMsgNotesLbl);

    auto *btnBox = new QFrame;
    btnBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
    auto *btnBoxLayout = new QHBoxLayout;
    btnBoxLayout->setContentsMargins(QMargins());
    btnBoxLayout->setSpacing(BGU::spi(15));
    btnBox->setLayout(btnBoxLayout);
    auto *closeBtn = new BlocknetFormBtn;
    closeBtn->setText(tr("Close"));
    btnBoxLayout->addWidget(closeBtn, 0, Qt::AlignCenter);

    auto *div1 = new BlocknetHDiv;
    auto *div2 = new BlocknetHDiv;
    auto *div3 = new BlocknetHDiv;

    layout->addSpacing(BGU::spi(20));
    layout->addWidget(titleLbl);
    layout->addSpacing(BGU::spi(10));
    layout->addWidget(descBox);
    layout->addSpacing(BGU::spi(10));
    layout->addWidget(div1);
    layout->addSpacing(BGU::spi(10));
    layout->addWidget(voteInfoLbl);
    layout->addSpacing(BGU::spi(10));
    layout->addWidget(voteInfoBox);
    layout->addSpacing(BGU::spi(10));
    layout->addWidget(div2);
    layout->addSpacing(BGU::spi(10));
    layout->addWidget(voteMsgLbl);
    layout->addSpacing(BGU::spi(10));
    layout->addWidget(voteMsgBox);
    layout->addSpacing(BGU::spi(10));
    layout->addWidget(div3);
    layout->addSpacing(BGU::spi(10));
    layout->addStretch(1);
    layout->addWidget(btnBox);
    layout->addSpacing(BGU::spi(10));

    connect(closeBtn, &BlocknetFormBtn::clicked, this, [this]() {
        close();
    });
}
