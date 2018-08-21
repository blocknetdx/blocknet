// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETPROPOSALS_H
#define BLOCKNETPROPOSALS_H

#include "blocknetvars.h"
#include "blocknetdropdown.h"

#include "walletmodel.h"

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVector>
#include <QDateTime>
#include <QScrollArea>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QDialog>
#include <QTimer>
#include <QMenu>

class BlocknetProposals : public QFrame
{
    Q_OBJECT

public:
    explicit BlocknetProposals(QFrame *parent = nullptr);
    void setWalletModel(WalletModel *w);

    enum statusflags {
        STATUS_PASSED = 0,
        STATUS_IN_PROGRESS = 1,
        STATUS_REJECTED = 2,
    };

    struct Proposal {
        uint256 hash;
        statusflags color;
        QString name;
        int superblock;
        int endblock;
        CAmount amount;
        int64_t payments;
        QString url;
        QString status;
        QString results;
        int vote;
        QString voteString;
    };

    void clear() {
        if (dataModel.count() > 0)
            table->clearContents();
    };

    QTableWidget* getTable() {
        return table;
    }

signals:
    //void createproposal();
    void tableUpdated();

public slots:
    //void onCreateProposal() { emit createproposal(); }
    void onVote();

private slots:
    void onItemChanged(QTableWidgetItem *item);
    void onFilter();
    void recordSelected();

private:
    QVBoxLayout *layout;
    WalletModel *walletModel;
    QLabel *titleLbl;
    QLabel *buttonLbl;
    QLabel *filterLbl;
    QTableWidget *table;
    QMenu *contextMenu;
    QTableWidgetItem *contextItem = nullptr;
    BlocknetDropdown *proposalsDropdown;
    QVector<Proposal> dataModel;
    QVector<Proposal> filteredData;
    QTimer *timer;
    bool syncInProgress = false;

    void initialize();
    void setData(QVector<Proposal> data);
    QVector<Proposal> filtered(int filter, int chainHeight);
    void unwatch();
    void watch();
    bool canVote();
    bool hasVote(const CTxIn &vin);
    void refresh(bool force = false);
    void showContextMenu(QPoint pt);

    enum {
        COLUMN_HASH,
        COLUMN_COLOR,
        COLUMN_PADDING1,
        COLUMN_NAME,
        COLUMN_SUPERBLOCK,
        COLUMN_AMOUNT,
        COLUMN_PAYMENT,
        COLUMN_URL,
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
    explicit BlocknetProposalsVoteDialog(BlocknetProposals::BlocknetProposals::Proposal &proposal, int displayUnit, QWidget *parent = nullptr);

signals:
    void submitVote(uint256 proposalHash, bool yes, bool no, bool abstain, bool voteMany);

protected:

private:
};

#endif // BLOCKNETPROPOSALS_H
