// Copyright (c) 2018-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETPROPOSALS_H
#define BLOCKNET_QT_BLOCKNETPROPOSALS_H

#include <qt/blocknetdropdown.h>
#include <qt/blocknetformbtn.h>
#include <qt/blocknetvars.h>

#include <qt/clientmodel.h>
#include <qt/walletmodel.h>

#include <governance/governance.h>
#include <validation.h>

#include <QDateTime>
#include <QDialog>
#include <QFrame>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>

class BlocknetProposals : public QFrame
{
    Q_OBJECT

public:
    explicit BlocknetProposals(QFrame *parent = nullptr);
    void setModels(ClientModel *c, WalletModel *w);

    enum statusflags {
        STATUS_PASSED = 0,
        STATUS_IN_PROGRESS = 1,
        STATUS_REJECTED = 2,
    };

    struct BlocknetProposal {
        uint256 hash;
        statusflags color;
        QString name;
        int superblock;
        CAmount amount;
        gov::Tally tally;
        QString url;
        QString description;
        QString status;
        QString results;
        gov::VoteType vote;
        QString voteString;
        CAmount voteAmount;
    };

    void clear() {
        if (dataModel.count() > 0)
            table->clearContents();
    };

    QTableWidget* getTable() {
        return table;
    }

Q_SIGNALS:
    void createProposal();
    void tableUpdated();

public Q_SLOTS:
    void onCreateProposal() {
        Q_EMIT createProposal();
    }
    void onVote();

private Q_SLOTS:
    void onItemChanged(QTableWidgetItem *item);
    void onFilter();
    void showProposalDetails(const BlocknetProposal & proposal);
    void setNumBlocks(int count, const QDateTime & blockDate, double nVerificationProgress, bool header);

private:
    QVBoxLayout *layout;
    ClientModel *clientModel;
    WalletModel *walletModel;
    QLabel *titleLbl;
    QLabel *filterLbl;
    QTableWidget *table;
    BlocknetFormBtn *voteBtn;
    QMenu *contextMenu;
    QTableWidgetItem *contextItem = nullptr;
    BlocknetDropdown *proposalsDropdown;
    QVector<BlocknetProposal> dataModel;
    QVector<BlocknetProposal> filteredData;
    QTimer *timer;
    int lastRow = -1;
    qint64 lastSelection = 0;
    bool syncInProgress = false;
    int lastVotes{0};

    void initialize();
    void setData(QVector<BlocknetProposal> data);
    QVector<BlocknetProposal> filtered(int filter, int chainHeight);
    void unwatch();
    void watch();
    bool canVote();
    void refresh(bool force = false);
    void showContextMenu(QPoint pt);
    BlocknetProposal proposalForHash(const QString & propHash);

    enum {
        COLUMN_HASH,
        COLUMN_COLOR,
        COLUMN_PADDING1,
        COLUMN_NAME,
        COLUMN_SUPERBLOCK,
        COLUMN_AMOUNT,
        COLUMN_TALLY,
        COLUMN_URL,
        COLUMN_DESCRIPTION,
        COLUMN_STATUS,
        COLUMN_RESULTS,
        COLUMN_VOTE,
        COLUMN_PADDING2
    };

    enum {
        FILTER_ALL,
        FILTER_ACTIVE,
        FILTER_UPCOMING,
        FILTER_COMPLETED
    };

    class NumberItem : public QTableWidgetItem {
    public:
        explicit NumberItem() = default;
        bool operator < (const QTableWidgetItem &other) const override {
            return amount < reinterpret_cast<const NumberItem*>(&other)->amount;
        };
        CAmount amount{0};
    };
};

class BlocknetProposalsVoteDialog : public QDialog {
    Q_OBJECT
public:
    explicit BlocknetProposalsVoteDialog(QVector<BlocknetProposals::BlocknetProposal> proposals, int displayUnit, QWidget *parent = nullptr);

Q_SIGNALS:
    void submitVotes(const std::vector<gov::ProposalVote> & votes);

private:
    std::map<uint256, QButtonGroup*> approveButtons;
};

class BlocknetProposalsDetailsDialog : public QDialog {
    Q_OBJECT
public:
    explicit BlocknetProposalsDetailsDialog(const BlocknetProposals::BlocknetProposal & proposal, int displayUnit, QWidget *parent = nullptr);

protected:

private:
};

#endif // BLOCKNET_QT_BLOCKNETPROPOSALS_H
