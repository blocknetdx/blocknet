// Copyright (c) 2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETACCOUNTS_H
#define BLOCKNET_QT_BLOCKNETACCOUNTS_H

#include <qt/blocknetactionbtn.h>
#include <qt/blocknetaddressbook.h>
#include <qt/blocknetfundsmenu.h>

#include <qt/walletmodel.h>

#include <QFrame>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

class BlocknetAccounts : public QFrame {
    Q_OBJECT
public:
    explicit BlocknetAccounts(QWidget *parent = nullptr);
    void setWalletModel(WalletModel *w);

    struct Account : public BlocknetAddressBook::Address {
        CAmount balance{0};
        int inputs{0};
    };

private:
    class NumberItem : public QTableWidgetItem {
    public:
        explicit NumberItem() = default;
        bool operator < (const QTableWidgetItem &other) const override {
            return amount < reinterpret_cast<const NumberItem*>(&other)->amount;
        };
        CAmount amount{0};
    };

    class LabelItem : public QTableWidgetItem {
    public:
        explicit LabelItem() = default;
        bool operator < (const QTableWidgetItem & other) const override {
            if (label.empty())
                return false;
            auto *oitem = reinterpret_cast<const LabelItem*>(&other);
            if (oitem->label.empty())
                return true;
            return label < oitem->label;
        };
        std::string label;
    };

Q_SIGNALS:
    void rescan(const std::string &);

private:
    WalletModel *walletModel;
    QVBoxLayout *layout;
    QLabel *titleLbl;
    QLabel *addButtonLbl;
    QTableWidget *table;
    QVector<Account> dataModel;
    QVector<Account> filteredData;

    void initialize();
    void setData(const QVector<Account> & data);
    void unwatch();
    void watch();

    enum {
        COLUMN_PADDING1,
        COLUMN_AVATAR,
        COLUMN_PADDING2,
        COLUMN_ALIAS,
        COLUMN_PADDING3,
        COLUMN_ADDRESS,
        COLUMN_PADDING4,
        COLUMN_BALANCE,
        COLUMN_PADDING5,
        COLUMN_INPUTS,
        COLUMN_PADDING6,
        COLUMN_COPY,
        COLUMN_EDIT,
    };

private Q_SLOTS:
    void onCopyAddress();
    void onAddAddress();
    void onEditAddress();
    void onDoubleClick(int row, int col);
};

#endif // BLOCKNET_QT_BLOCKNETACCOUNTS_H
