// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETDASHBOARD_H
#define BLOCKNET_QT_BLOCKNETDASHBOARD_H

#include <qt/blocknetvars.h>

#include <qt/optionsmodel.h>
#include <qt/transactionrecord.h>
#include <qt/transactiontablemodel.h>
#include <qt/walletmodel.h>

#include <QDateTime>
#include <QFrame>
#include <QLabel>
#include <QModelIndex>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QTableWidget>
#include <QVariant>
#include <QVBoxLayout>
#include <QVector>

class BlocknetDashboardFilterProxy;

class BlocknetDashboard : public QFrame
{
    Q_OBJECT

public:
    explicit BlocknetDashboard(QFrame *parent = nullptr);
    void setWalletModel(WalletModel *w);

    enum {
        COLUMN_STATUS,
        COLUMN_DATE,
        COLUMN_TIME,
        COLUMN_PADDING1,
        COLUMN_TOADDRESS,
        COLUMN_PADDING2,
        COLUMN_TYPE,
        COLUMN_PADDING3,
        COLUMN_AMOUNT,
        COLUMN_PADDING4,
    };

    struct DashboardTx {
        COutPoint outpoint;
        int status;
        qint64 datetime;
        QString date;
        QString time;
        QString address;
        QString type;
        QString amount;
        QColor amountColor;
        QString tooltip;
    };

Q_SIGNALS:
    void quicksend();
    void history();

public Q_SLOTS:
    void onQuickSend() { Q_EMIT quicksend(); }
    void balanceChanged(const interfaces::WalletBalances & balances);
    void displayUnitChanged(int displayUnit);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private Q_SLOTS:
    void onViewAll() { Q_EMIT history(); };

private:
    void initialize();
    void setData(const QVector<DashboardTx> & data);
    void updateData(int row, TransactionTableModel *tableModel, DashboardTx & d);
    void addData(int row, const DashboardTx & d);
    void refreshTableData();

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
    QTableWidget *table;
    QVector<DashboardTx> dataModel;
    BlocknetDashboardFilterProxy *filter;

    void updateBalance();
    void walletEvents(bool on);
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

#endif // BLOCKNET_QT_BLOCKNETDASHBOARD_H
