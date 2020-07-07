// Copyright (c) 2018-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetproposals.h>

#include <qt/blocknetcheckbox.h>
#include <qt/blocknetcreateproposal.h>
#include <qt/blocknetdropdown.h>
#include <qt/blocknetguiutil.h>
#include <qt/blocknetformbtn.h>
#include <qt/blocknethdiv.h>
#include <qt/blocknetvars.h>

#include <qt/bitcoinunits.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>

#include <governance/governancewallet.h>
#include <net.h>
#include <uint256.h>
#include <wallet/coincontrol.h>

#include <utility>

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QFormLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QRadioButton>
#include <QSettings>
#include <QVariant>

int getChainHeight() {
    int height;
    {
        LOCK(cs_main);
        height = chainActive.Height();
    }
    return height;
}

BlocknetProposals::BlocknetProposals(QFrame *parent) : QFrame(parent), layout(new QVBoxLayout),
                                                       clientModel(nullptr), walletModel(nullptr),
                                                       contextMenu(new QMenu)
{
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
    filterLbl = new QLabel(tr("Filter by:"));
    filterLbl->setObjectName("title");
    QStringList list{tr("All Proposals"), tr("Current"), tr("Upcoming"), tr("Completed")};
    proposalsDropdown = new BlocknetDropdown(list);
    topBoxLayout->addStretch(1);
    topBoxLayout->addWidget(filterLbl);
    topBoxLayout->addWidget(proposalsDropdown);

    table = new QTableWidget;
    table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    table->setContentsMargins(QMargins());
    table->setColumnCount(COLUMN_PADDING2 + 1);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setFocusPolicy(Qt::NoFocus);
    table->setAlternatingRowColors(true);
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
    table->setColumnHidden(COLUMN_HASH, true); table->setColumnWidth(COLUMN_HASH, BGU::spi(0));
    table->horizontalHeader()->setSectionResizeMode(COLUMN_COLOR, QHeaderView::Fixed); table->setColumnWidth(COLUMN_COLOR, BGU::spi(3));
    table->horizontalHeader()->setSectionResizeMode(COLUMN_PADDING1, QHeaderView::Fixed); table->setColumnWidth(COLUMN_PADDING1, BGU::spi(1));
    table->horizontalHeader()->setSectionResizeMode(COLUMN_SUPERBLOCK, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_NAME, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_AMOUNT, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_TALLY, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_URL, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_DESCRIPTION, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_RESULTS, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_VOTE, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_PADDING2, QHeaderView::Fixed); table->setColumnWidth(COLUMN_PADDING2, BGU::spi(2));
    table->setHorizontalHeaderLabels({ "", "", "", tr("Name"), tr("Superblock"), tr("Amount"), tr("Y-N-A"), tr("Url"), tr("Description"), tr("Status"), tr("Results"), "", ""});
    table->setColumnWidth(COLUMN_TALLY, BGU::spi(60));

    auto *btnBox = new QFrame;
    btnBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
    auto *btnBoxLayout = new QHBoxLayout;
    btnBoxLayout->setContentsMargins(QMargins());
    btnBox->setLayout(btnBoxLayout);
    voteBtn = new BlocknetFormBtn;
    voteBtn->setObjectName("delete");
    voteBtn->setText(tr("Cast Votes"));
    auto *createProposalBtn = new BlocknetFormBtn;
    createProposalBtn->setText(tr("Create New Proposal"));
    btnBoxLayout->addWidget(voteBtn);
    btnBoxLayout->addStretch(1);
    btnBoxLayout->addWidget(createProposalBtn);

    auto *titleBox = new QFrame;
    titleBox->setContentsMargins(QMargins());
    auto *titleBoxLayout = new QHBoxLayout;
    titleBox->setLayout(titleBoxLayout);
    titleBoxLayout->addWidget(titleLbl);
    titleBoxLayout->addStretch(1);
    titleBoxLayout->addWidget(topBox);

    layout->addWidget(titleBox);
    layout->addSpacing(BGU::spi(15));
    layout->addWidget(table, 1);
    layout->addSpacing(BGU::spi(15));
    layout->addWidget(btnBox);
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

    QSettings settings;
    auto filterIdx = settings.value("proposalsFilter", FILTER_ACTIVE);
    proposalsDropdown->setCurrentIndex(filterIdx.toInt());

    // Timer used to check for vote capabilities and refresh proposals
    int timerInterval = 5000;
    timer = new QTimer(this);
    timer->setInterval(timerInterval);
    connect(timer, &QTimer::timeout, this, [this]() {
        if (!voteBtn || !walletModel)
            return;
        bool cannotVote = !canVote();
        voteBtn->setEnabled(!cannotVote);
        if (cannotVote) table->setColumnWidth(COLUMN_VOTE, 0);
        else table->setColumnWidth(COLUMN_VOTE, BGU::spi(150));
        table->setColumnHidden(COLUMN_VOTE, cannotVote);
        refresh(false);
    });
    timer->start(timerInterval);

    connect(createProposalBtn, &BlocknetFormBtn::clicked, this, &BlocknetProposals::onCreateProposal);
    connect(voteBtn, &BlocknetFormBtn::clicked, this, &BlocknetProposals::onVote);

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
        auto *hashItem = table->item(row, COLUMN_HASH);
        if (hashItem)
            showProposalDetails(proposalForHash(hashItem->data(Qt::DisplayRole).toString()));
    });
    connect(table, &QTableWidget::customContextMenuRequested, this, &BlocknetProposals::showContextMenu);
    connect(proposalsDropdown, &BlocknetDropdown::valueChanged, this, &BlocknetProposals::onFilter);

    connect(viewDetails, &QAction::triggered, this, [this]() {
        if (contextItem == nullptr || contextItem->row() >= filteredData.size())
            return;
        auto *hashItem = table->item(contextItem->row(), COLUMN_HASH);
        if (hashItem)
            showProposalDetails(proposalForHash(hashItem->data(Qt::DisplayRole).toString()));
    });
    connect(copyName, &QAction::triggered, this, [this]() {
        if (contextItem == nullptr || contextItem->row() >= filteredData.size())
            return;
        auto *nameItem = table->item(contextItem->row(), COLUMN_NAME);
        if (nameItem)
            QApplication::clipboard()->setText(nameItem->data(Qt::DisplayRole).toString(), QClipboard::Clipboard);
    });
    connect(copyUrl, &QAction::triggered, this, [this]() {
        if (contextItem == nullptr || contextItem->row() >= filteredData.size())
            return;
        auto *urlItem = table->item(contextItem->row(), COLUMN_URL);
        if (urlItem)
            QApplication::clipboard()->setText(urlItem->data(Qt::DisplayRole).toString(), QClipboard::Clipboard);
    });
    connect(copyHash, &QAction::triggered, this, [this]() {
        if (contextItem == nullptr || contextItem->row() >= filteredData.size())
            return;
        auto *hashItem = table->item(contextItem->row(), COLUMN_HASH);
        if (hashItem) {
            auto propHash = hashItem->data(Qt::DisplayRole).toString();
            QApplication::clipboard()->setText(propHash, QClipboard::Clipboard);
        }
    });
    connect(copyYes, &QAction::triggered, this, [this]() {
        if (contextItem == nullptr || contextItem->row() >= filteredData.size())
            return;
        auto *hashItem = table->item(contextItem->row(), COLUMN_HASH);
        if (hashItem) {
            auto propHash = hashItem->data(Qt::DisplayRole).toString();
            QApplication::clipboard()->setText(QString("vote %1 yes").arg(propHash), QClipboard::Clipboard);
        }
    });
    connect(copyNo, &QAction::triggered, this, [this]() {
        if (contextItem == nullptr || contextItem->row() >= filteredData.size())
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
        superblockResults[superblock] = gov::Governance::instance().getSuperblockResults(superblock, Params().GetConsensus(), true);
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

        gov::Tally tally;
        auto it = sbResults.find(proposal);
        if (it != sbResults.end())
            tally = it->second;

        if (nextSuperblock < proposal.getSuperblock()) {
            results = tr("Pending");
            statusColor = STATUS_IN_PROGRESS;
        }
        else if (currentBlock < proposal.getSuperblock() && tally.payout) {
            results = tr("Passing");
            statusColor = STATUS_PASSED;
        }
        else if (currentBlock < proposal.getSuperblock() && !tally.payout) {
            results = tr("Failing");
            statusColor = STATUS_REJECTED;
        }
        else if (currentBlock >= proposal.getSuperblock() && tally.payout) {
            results = tr("Passed");
            status = tr("Finished");
            statusColor = STATUS_PASSED;
        }
        else if (currentBlock >= proposal.getSuperblock() && !tally.payout) {
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
            const auto castVote = gov::GetMyVotes(proposal.getHash(), wallets, Params().GetConsensus());
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
            tally,
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

    onFilter();
}

void BlocknetProposals::setModels(ClientModel *c, WalletModel *w) {
    if (walletModel == w && clientModel == c)
        return;

    walletModel = w;
    if (!walletModel || !walletModel->getOptionsModel())
        return;

    if (clientModel)
        disconnect(clientModel, &ClientModel::numBlocksChanged, this, &BlocknetProposals::setNumBlocks);

    clientModel = c;
    if (!clientModel)
        return;
    connect(clientModel, &ClientModel::numBlocksChanged, this, &BlocknetProposals::setNumBlocks);

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

        // tally
        auto *tallyItem = new QTableWidgetItem;
        tallyItem->setData(Qt::DisplayRole, QString("%1-%2-%3").arg(QString::number(d.tally.yes),
                QString::number(d.tally.no), QString::number(d.tally.abstain)));
        table->setItem(i, COLUMN_TALLY, tallyItem);

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
        voteLbl->setToolTip(tr("Vote status updates as votes are confirmed"));
        if (!voteText.isEmpty()) {
            voteLbl->setText(voteText);
            boxLayout->addWidget(voteLbl, 0, Qt::AlignCenter);
            table->setRowHeight(i, BGU::spi(50));
        } else {
            voteLbl->setText(tr("Did not vote"));
            boxLayout->addWidget(voteLbl, 0, Qt::AlignCenter);
        }

        table->setRowHeight(i, BGU::spi(50));
        table->setCellWidget(i, COLUMN_VOTE, widget);
        table->setItem(i, COLUMN_VOTE, votedItem);

        // padding 2
        auto *pad2Item = new QTableWidgetItem;
        table->setItem(i, COLUMN_PADDING2, pad2Item);
    }
    // Hide the vote column if we're not able to vote
    bool cannotVote = !canVote();
    voteBtn->setEnabled(!cannotVote);
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
    const auto balance = walletModel->wallet().getBalance() + walletModel->wallet().getImmatureBalance()
            + walletModel->wallet().getUnconfirmedBalance();
    CCoinControl cc;
    const auto available = walletModel->wallet().getAvailableBalance(cc);
    return  balance > Params().GetConsensus().voteBalance && available > 0;
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
    QSettings settings;
    settings.setValue("proposalsFilter", proposalsDropdown->currentIndex());
}

void BlocknetProposals::onVote() {
    QVector<BlocknetProposals::BlocknetProposal> proposals;
    const auto nextSB = gov::Governance::nextSuperblock(Params().GetConsensus());
    for (const auto & prop : dataModel) {
        if (prop.superblock >= nextSB)
            proposals.push_back(prop);
    }
    if (proposals.empty()) { // do nothing if no proposal found
        QMessageBox::warning(this, tr("No proposals found"), tr("There are no proposals for the current voting period"));
        return;
    }

    auto *dialog = new BlocknetProposalsVoteDialog(proposals, walletModel->getOptionsModel()->getDisplayUnit());
    dialog->setStyleSheet(GUIUtil::loadStyleSheet());
    connect(dialog, &BlocknetProposalsVoteDialog::submitVotes, this, [this, dialog](const std::vector<gov::ProposalVote> & votes) {
        QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Cast Votes"),
                tr("Are you sure you want to cast these votes?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (retval != QMessageBox::Yes)
            return;

        WalletModel::UnlockContext ctx(walletModel->requestUnlock());
        if (!ctx.isValid()) {
            return;
        }
        // Cast votes
        std::string failureReason;
        std::vector<CTransactionRef> txns;
        if (!gov::SubmitVotes(votes, GetWallets(), Params().GetConsensus(), txns, g_connman.get(), &failureReason))
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

void BlocknetProposals::setNumBlocks(int count, const QDateTime &blockDate, double nVerificationProgress,
                                     bool header)
{
    // Only refresh proposal data if the cache changes
    const auto newVotes = static_cast<int>(gov::Governance::instance().getVotes(gov::NextSuperblock(Params().GetConsensus(), count)).size());
    const auto newProposals = static_cast<int>(gov::Governance::instance().getProposals().size());
    if (lastVotes != newVotes || dataModel.size() != newProposals) {
        lastVotes = newVotes;
        // refresh data
        initialize();
        onFilter();
    }
}

BlocknetProposals::BlocknetProposal BlocknetProposals::proposalForHash(const QString & propHash) {
    const auto & hash = uint256S(propHash.toStdString());
    for (const auto & proposal : filteredData) {
        if (proposal.hash == hash)
            return proposal;
    }
    return BlocknetProposal{};
}

/**
 * Dialog that manages the vote submission for the specified proposal.
 * @param proposal Proposal that's being voting on
 * @param parent Optional parent widget to attach to
 */
BlocknetProposalsVoteDialog::BlocknetProposalsVoteDialog(QVector<BlocknetProposals::BlocknetProposal> proposals,
        int displayUnit, QWidget *parent) : QDialog(parent)
{
    auto *layout = new QVBoxLayout;
    layout->setContentsMargins(BGU::spi(30), BGU::spi(10), BGU::spi(30), BGU::spi(10));
    this->setLayout(layout);
    this->setMinimumWidth(BGU::spi(650));
    this->setWindowTitle(tr("Voting"));

    auto *titleLbl = new QLabel(tr("Vote on Proposals"));
    titleLbl->setObjectName("h2");
    titleLbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    auto *proposalGrid = new QFrame;
    proposalGrid->setObjectName("contentFrame");
    proposalGrid->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    auto *gridLayout = new QGridLayout;
    gridLayout->setContentsMargins(QMargins());
    gridLayout->setVerticalSpacing(BGU::spi(8));
    proposalGrid->setLayout(gridLayout);

    auto *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setContentsMargins(QMargins());
    scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    scrollArea->setWidget(proposalGrid);
    scrollArea->setMinimumHeight(BGU::spi(250));

    auto *nameLbl = new QLabel(tr("Proposal"));           nameLbl->setObjectName("description");
    auto *superblockLbl = new QLabel(tr("Superblock"));   superblockLbl->setObjectName("description");
    auto *amountLbl = new QLabel(tr("Amount"));           amountLbl->setObjectName("description");
    auto *approveLbl = new QLabel(tr("Do you approve?")); approveLbl->setObjectName("description");
    auto *pl = new QLabel;
    auto *headerDiv = new BlocknetHDiv; headerDiv->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    gridLayout->addWidget(nameLbl,        0, 0, Qt::AlignTop | Qt::AlignLeft);
    gridLayout->addWidget(superblockLbl,  0, 1, Qt::AlignTop | Qt::AlignLeft);
    gridLayout->addWidget(amountLbl,      0, 2, Qt::AlignTop | Qt::AlignLeft);
    gridLayout->addWidget(pl,             0, 3); gridLayout->setColumnStretch(3, 1);
    gridLayout->addWidget(approveLbl,     0, 4, Qt::AlignTop | Qt::AlignCenter);
    gridLayout->addWidget(pl,             0, 5); gridLayout->setColumnMinimumWidth(5, BGU::spi(5));
    gridLayout->addWidget(headerDiv,      1, 0, 1, 5, Qt::AlignTop);
    gridLayout->setVerticalSpacing(BGU::spi(8));
    gridLayout->setHorizontalSpacing(BGU::spi(15));

    int row = 2; // account for header + div rows
    auto chainHeight = getChainHeight();

    std::sort(proposals.begin(), proposals.end(), [](const BlocknetProposals::BlocknetProposal & a, const BlocknetProposals::BlocknetProposal & b) -> bool {
        return a.superblock < b.superblock; // ascending (later proposals last)
    });
    for (int i = 0; i < proposals.size(); ++i) {
        const auto & proposal = proposals[i];
        QString amount = BitcoinUnits::floorWithUnit(displayUnit, proposal.amount);
        QString superblock = QString::number(proposal.superblock);
        auto *nameVal = new QLabel(proposal.name);    nameVal->setObjectName("value");       nameVal->setTextInteractionFlags(Qt::TextSelectableByMouse);
        auto *superblockVal = new QLabel(superblock); superblockVal->setObjectName("value"); superblockVal->setTextInteractionFlags(Qt::TextSelectableByMouse);
        auto *amountVal = new QLabel(amount);         amountVal->setObjectName("value");     amountVal->setTextInteractionFlags(Qt::TextSelectableByMouse);

        auto *approveBox = new QFrame;
        auto *approveBoxLayout = new QVBoxLayout;
        approveBox->setContentsMargins(QMargins());
        approveBox->setLayout(approveBoxLayout);
        auto *rbBoxLayout = new QHBoxLayout;
        rbBoxLayout->setSpacing(BGU::spi(20));
        auto *yesRb = new QRadioButton(tr("Yes"));
        auto *noRb = new QRadioButton(tr("No"));
        auto *abstainRb = new QRadioButton(tr("Abstain"));
        auto *group = new QButtonGroup;

        // Disable widgets on proposals in the voting cutoff period.
        if (chainHeight >= proposal.superblock - Params().GetConsensus().votingCutoff) {
            yesRb->setDisabled(true);
            noRb->setDisabled(true);
            abstainRb->setDisabled(true);
            yesRb->setToolTip(tr("Voting period for this proposal has ended"));
            noRb->setToolTip(tr("Voting period for this proposal has ended"));
            abstainRb->setToolTip(tr("Voting period for this proposal has ended"));
        }

        group->addButton(yesRb, 0);
        group->addButton(noRb, 1);
        group->addButton(abstainRb, 2);
        rbBoxLayout->addWidget(yesRb);
        rbBoxLayout->addWidget(noRb);
        rbBoxLayout->addWidget(abstainRb);
        approveBoxLayout->addLayout(rbBoxLayout);

        gridLayout->addWidget(nameVal,        row, 0);
        gridLayout->addWidget(superblockVal,  row, 1);
        gridLayout->addWidget(amountVal,      row, 2);
        gridLayout->addWidget(pl,             row, 3);
        gridLayout->addWidget(approveBox,     row, 4);
        gridLayout->addWidget(pl,             row, 5);
        auto *div = new BlocknetHDiv; div->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        gridLayout->addWidget(div, row+1, 0, 1, 5, Qt::AlignTop);

        // Store ref to button groups
        approveButtons[proposal.hash] = group;

        row += 2;
    }
    // Grid should be condensed, put remaining space after last item
    gridLayout->setRowStretch(row, 1);

    auto *voteDescBox = new QFrame;
    auto *voteDescBoxLayout = new QVBoxLayout;
    voteDescBox->setContentsMargins(QMargins());
    voteDescBoxLayout->setContentsMargins(QMargins());
    voteDescBox->setLayout(voteDescBoxLayout);
    auto *voteTitleLbl = new QLabel(tr("Network Fee"));
    voteTitleLbl->setObjectName("h5");
    auto *voteDescLbl = new QLabel(tr("Casting votes will submit a transaction to the network where you'll be charged "
                                      "a minimal network fee in order to secure your vote on-chain. The ideal way to "
                                      "vote is to put a small 1 BLOCK UTXO in each voting address so that this is used "
                                      "to pay the network fee (instead of larger voting inputs)."));
    voteDescLbl->setWordWrap(true);
    voteDescLbl->setObjectName("description");
    voteDescBoxLayout->addWidget(voteTitleLbl);
    voteDescBoxLayout->addWidget(voteDescLbl);

    auto *btnBox = new QFrame;
    btnBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
    auto *btnBoxLayout = new QHBoxLayout;
    btnBoxLayout->setContentsMargins(QMargins());
    btnBoxLayout->setSpacing(BGU::spi(15));
    btnBox->setLayout(btnBoxLayout);
    auto *voteBtn = new BlocknetFormBtn;
    voteBtn->setText(tr("Submit Votes"));
    auto *cancelBtn = new BlocknetFormBtn;
    cancelBtn->setObjectName("cancel");
    cancelBtn->setText(tr("Cancel"));
    btnBoxLayout->addWidget(cancelBtn, 0, Qt::AlignRight | Qt::AlignBottom);
    btnBoxLayout->addWidget(voteBtn, 0, Qt::AlignLeft | Qt::AlignBottom);

    auto *div1 = new BlocknetHDiv;

    layout->addSpacing(BGU::spi(20));
    layout->addWidget(titleLbl);
    layout->addSpacing(BGU::spi(10));
    layout->addWidget(scrollArea, 1);
    layout->addSpacing(BGU::spi(20));
    layout->addWidget(voteDescBox);
    layout->addSpacing(BGU::spi(10));
    layout->addWidget(btnBox);
    layout->addSpacing(BGU::spi(10));

    connect(voteBtn, &BlocknetFormBtn::clicked, this, [this]() {
        std::vector<gov::ProposalVote> votes;
        for (const auto & item : approveButtons) {
            const int bid = item.second->checkedId();
            if (bid >= 0) { // skip non-voting proposals
                const auto & proposal = gov::Governance::instance().getProposal(item.first);
                if (!proposal.isNull())
                    votes.emplace_back(proposal, bid == 0 ? gov::VoteType::YES : bid == 1 ? gov::VoteType::NO : gov::VoteType::ABSTAIN);
            }
        }
        if (votes.empty()) {
            QMessageBox::critical(this, tr("Failed to vote"), tr("Please select a vote option (Yes, No, Abstain)"));
            return;
        }
        Q_EMIT submitVotes(votes);
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
                                          "incur minimal network fees. The ideal way to vote is to put a small 1 BLOCK UTXO "
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
