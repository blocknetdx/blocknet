// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETADDRESSBOOK_H
#define BLOCKNETADDRESSBOOK_H

#include "walletmodel.h"
#include "blocknetdropdown.h"
#include "blocknetactionbtn.h"
#include "blocknetfundsmenu.h"

#include <QFrame>
#include <QVBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>

class BlocknetAddressBook : public QFrame 
{
    Q_OBJECT

public:
    explicit BlocknetAddressBook(QWidget *popup, QFrame *parent = nullptr);
    void setWalletModel(WalletModel *w);

    struct Address {
        QString alias;
        QString address;
    };

private:
    WalletModel *walletModel;
    QVBoxLayout *layout;
    QLabel *titleLbl;
    QLabel *addButtonLbl;
    QLabel *filterLbl;
    BlocknetDropdown *addressDropdown;
    QTableWidget *table;
    QVector<Address> dataModel;
    QVector<Address> filteredData;
    BlocknetFundsMenu *fundsMenu = nullptr;
    QWidget *popupWidget;

    void initialize();
    void setData(QVector<Address> data);
    QVector<Address> filtered(int filter, int chainHeight);
    void unwatch();
    void watch();

    enum {
        COLUMN_ACTION,
        COLUMN_AVATAR,
        COLUMN_ALIAS,
        COLUMN_ADDRESS,
        COLUMN_COPY,
        COLUMN_EDIT,
        COLUMN_DELETE
    };

    enum {
        FILTER_ALL,
        FILTER_SENDING,
        FILTER_RECEIVING
    };

public slots:
    void onAddressAction();

signals:
    void sendFunds();
    void requestFunds();
};

#endif // BLOCKNETADDRESSBOOK_H
