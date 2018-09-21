// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETTRANSACTIONHISTORY_H
#define BLOCKNETTRANSACTIONHISTORY_H

#include "blocknetdropdown.h"
#include "blocknetlineedit.h"

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
#include <QMenu>
#include <QDateTimeEdit>
#include <QKeyEvent>

class BlocknetTransactionHistoryTable;

class BlocknetTransactionHistory : public QFrame {
    Q_OBJECT

public:
    explicit BlocknetTransactionHistory(WalletModel *w, QWidget *parent = nullptr);

    // Date ranges for filter
    enum {
        All,
        Today,
        ThisWeek,
        ThisMonth,
        LastMonth,
        ThisYear,
        Range
    };

signals:

public slots:

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
    void onDisplayUnit(int);
    void addressChanged(const QString &prefix);
    void amountChanged(const QString &amount);
    void dateChanged(int idx);
    void typeChanged(int idx);
    void dateRangeChanged();
    void contextualMenu(const QPoint& point);
    void copyAddress();
    void copyLabel();
    void copyAmount();
    void copyTxID();
    void showDetails();
    void exportClicked();
    void displayTotalSelected(const QItemSelection &, const QItemSelection &);

private:
    WalletModel *walletModel;
    int displayUnit;
    QVBoxLayout *layout;
    QLabel *titleLbl;
    BlocknetTransactionHistoryTable *transactionsTbl;
    BlocknetDropdown *dateCb;
    BlocknetDropdown *typeCb;
    BlocknetLineEdit *addressTi;
    BlocknetLineEdit *amountTi;
    QMenu *contextMenu;
    QFrame *dateRangeWidget;
    QDateTimeEdit *dateFrom;
    QDateTimeEdit *dateTo;
    QLabel *totalSelectedLbl;
};

class BlocknetTransactionHistoryTable : public QTableView {
    Q_OBJECT

public:
    explicit BlocknetTransactionHistoryTable(QWidget *parent = nullptr);
    void setWalletModel(WalletModel *w);
    void leave();
    void enter();
    void setAddressPrefix(const QString &prefix);
    void setMinAmount(const CAmount &minimum);
    void setTypeFilter(quint32 types);
    void setDateRange(const QDateTime &from, const QDateTime &to);

private:
    WalletModel *walletModel;
};

class BlocknetTransactionHistoryFilterProxy : public QSortFilterProxyModel {
    Q_OBJECT

public:
    explicit BlocknetTransactionHistoryFilterProxy(OptionsModel *o, QObject *parent = nullptr);

    QVariant data(const QModelIndex &index, int role) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &) const override;

    static const QDateTime MIN_DATE; // Earliest date that can be represented (far in the past)
    static const QDateTime MAX_DATE; // Last date that can be represented (far in the future)
    static const quint32 ALL_TYPES = 0xFFFFFFFF; // Type filter bit field (all types)
    static const quint32 COMMON_TYPES = 4479; // Type filter bit field (all types but Obfuscation-SPAM)
    static quint32 TYPE(int type) { return 1 << type; }

    enum ColumnIndex {
        HistoryStatus = 0,
        HistoryDate = 1,
        HistoryTime = 2,
        HistoryToAddress = 3,
        HistoryType = 4,
        HistoryAmount = 5
    };

    void setLimit(int limit); // Set maximum number of rows returned, -1 if unlimited.
    void setAddressPrefix(const QString &prefix);
    void setMinAmount(const CAmount &minimum);
    void setTypeFilter(quint32 types);
    void setDateRange(const QDateTime &from, const QDateTime &to);

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    OptionsModel *optionsModel;
    int limitRows;
    QString addrPrefix;
    CAmount minAmount;
    quint32 typeFilter;
    QDateTime dateFrom;
    QDateTime dateTo;
};

#include <QStyledItemDelegate>

/**
 * @brief Responsible for drawing the custom table columns (Status, Date, Amount).
 * @param o
 * @param parent
 */
class BlocknetTransactionHistoryCellItem : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit BlocknetTransactionHistoryCellItem(QObject *parent = nullptr);

protected:
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

#endif // BLOCKNETTRANSACTIONHISTORY_H