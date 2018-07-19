// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETPROPOSALS_H
#define BLOCKNETPROPOSALS_H

#include "blocknetvars.h"
#include "walletmodel.h"

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVector>
#include <QDateTime>
#include <QScrollArea>
#include <QTableWidget>

class BlocknetProposals : public QFrame
{
    Q_OBJECT

public:
    explicit BlocknetProposals(QFrame *parent = nullptr);
    void setWalletModel(WalletModel *w);
    void initialize();

    enum statusflags {
        STATUS_PASSED = 0,
        STATUS_IN_PROGRESS = 1,
        STATUS_REJECTED = 2,
    };

    enum voteflags {
        VOTE_UNDEFINED = 0,
        VOTE_YES = 1,
        VOTE_NO = 2,
    };

    struct UTXO {
        statusflags color;
        QString name;
        int superblock;
        QString amount;
        int64_t payments;
        QString status;
        QString results;
        voteflags vote;
        QString voteString;
    };
    void setData(QVector<UTXO> data);
    QVector<UTXO> getData();

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

private slots:
    void onItemChanged(QTableWidgetItem *item);
    void recordSelected();

private:
    QLabel *titleLbl;
    QLabel *buttonLbl;
    QLabel *filterLbl;
    QTableWidget *table;
    QTableWidgetItem *contextItem = nullptr;
    QVector<UTXO> dataModel;
    QVBoxLayout *layout;
    WalletModel *walletModel;

    void unwatch();
    void watch();

    enum {
        COLUMN_COLOR,
        COLUMN_NAME,
        COLUMN_SUPERBLOCK,
        COLUMN_AMOUNT,
        COLUMN_PAYMENT,
        COLUMN_STATUS,
        COLUMN_RESULTS,
        COLUMN_VOTE
    };
};

#endif // BLOCKNETPROPOSALS_H
