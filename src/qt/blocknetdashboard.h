// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETDASHBOARD_H
#define BLOCKNETDASHBOARD_H

#include "blocknetvars.h"

#include "walletmodel.h"
#include "optionsmodel.h"
#include "transactionrecord.h"
#include "transactiontablemodel.h"

#include <QFrame>
#include <QTableView>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVector>
#include <QDateTime>
#include <QScrollArea>
#include <QVector>
#include <QSortFilterProxyModel>
#include <QVariant>
#include <QModelIndex>
#include <QPainter>

class BlocknetDashboardTable;

class BlocknetDashboard : public QFrame
{
    Q_OBJECT

public:
    explicit BlocknetDashboard(QFrame *parent = nullptr);
    void setWalletModel(WalletModel *w);

signals:
    void quicksend();
    void history();

public slots:
    void onQuickSend() { emit quicksend(); }
    void balanceChanged(CAmount walletBalance, CAmount unconfirmed, CAmount immature, CAmount anonymized, CAmount watch,
                        CAmount watchUnconfirmed, CAmount watchImmature);
    void displayUnitChanged(int displayUnit);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
    void onViewAll() { emit history(); };

private:
    QVBoxLayout *layout;
    WalletModel *walletModel;
    int displayUnit;
    CAmount walletBalance;
    CAmount unconfirmedBalance;
    CAmount immatureBalance;

    QLabel *titleLbl;
    QLabel *balanceLbl;
    QLabel *balanceValueLbl;
    QLabel *pendingLbl;
    QLabel *pendingValueLbl;
    QLabel *immatureLbl;
    QLabel *immatureValueLbl;
    QLabel *totalLbl;
    QLabel *totalValueLbl;
    QPushButton *viewAll;
    QLabel *recentTxsLbl;
    QFrame *recentTransactions;
    BlocknetDashboardTable *transactionsTbl;

    void updateBalance();
    void walletEvents(bool on);
};

class BlocknetDashboardTable : public QTableView {
    Q_OBJECT

public:
    explicit BlocknetDashboardTable(QWidget *parent = nullptr);
    void setWalletModel(WalletModel *w);
    void leave();
    void enter();

signals:

public slots:

private:
    WalletModel *walletModel;
};

class BlocknetDashboardFilterProxy : public QSortFilterProxyModel {
    Q_OBJECT

public:
    explicit BlocknetDashboardFilterProxy(OptionsModel *o, QObject *parent = nullptr);

    QVariant data(const QModelIndex &index, int role) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &) const override;

    enum ColumnIndex {
        DashboardStatus = 0,
        DashboardDate = 1,
        DashboardTime = 2,
        DashboardToAddress = 3,
        DashboardType = 4,
        DashboardAmount = 5
    };

    void setLimit(int limit); /** Set maximum number of rows returned, -1 if unlimited. */

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    OptionsModel *optionsModel;
    int limitRows;
    static quint32 TYPE(int type) { return 1 << type; }
};

#include <QStyledItemDelegate>

/**
 * @brief Responsible for drawing the custom dashboard table columns (Status, Date, Amount).
 * @param o
 * @param parent
 */
class BlocknetDashboardCellItem : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit BlocknetDashboardCellItem(QObject *parent = nullptr);

protected:
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

#endif // BLOCKNETDASHBOARD_H
