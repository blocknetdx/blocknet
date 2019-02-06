// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETADDRESSBOOK_H
#define BLOCKNETADDRESSBOOK_H

#include "blocknetdropdown.h"
#include "blocknetactionbtn.h"
#include "blocknetfundsmenu.h"

#include "walletmodel.h"

#include <QFrame>
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QDialog>
#include <QResizeEvent>

class BlocknetAddressBook : public QFrame 
{
    Q_OBJECT

public:
    explicit BlocknetAddressBook(bool slimMode = false, int filter = FILTER_DEFAULT, QWidget *parent = nullptr);
    void setWalletModel(WalletModel *w);

    struct Address {
        QString alias;
        QString address;
        int type;
    };

    enum {
        FILTER_DEFAULT = -1, // default filter state
        FILTER_SENDING = 0,
        FILTER_RECEIVING = 1,
        FILTER_ALL = 2, // must not collide with AddressTableEntry::Type
    };

private:
    bool slimMode;
    WalletModel *walletModel;
    QVBoxLayout *layout;
    QLabel *titleLbl;
    QLabel *addButtonLbl;
    QLabel *filterLbl;
    BlocknetDropdown *addressDropdown;
    QTableWidget *table;
    QVector<Address> dataModel;
    QVector<Address> filteredData;
    int filteredOption = -1;

    void initialize();
    void setData(const QVector<Address> &data);
    QVector<Address> filtered(const QVector<Address> &data, int filter);
    void unwatch();
    void watch();
    int ddIndexForType(int type);

    enum {
        COLUMN_ACTION,
        COLUMN_AVATAR,
        COLUMN_ALIAS,
        COLUMN_ADDRESS,
        COLUMN_COPY,
        COLUMN_EDIT,
        COLUMN_DELETE,
    };

public slots:
    void onAddressAction();

signals:
    void send(const QString &);

private slots:
    void onFilter();
    void onCopyAddress();
    void onAddAddress();
    void onEditAddress();
    void onDeleteAddress();
    void onDoubleClick(int row, int col);
};

class BlocknetAddressBookDialog : public QDialog {
    Q_OBJECT
public:
    explicit BlocknetAddressBookDialog(WalletModel *model, Qt::WindowFlags f, int filter = BlocknetAddressBook::FILTER_DEFAULT, QWidget *parent = nullptr);
    void singleShotMode() { ssMode = true; } // this "accepts" the dialog on select address
    BlocknetAddressBook *form;
signals:
    void send(const QString &);
protected:
    void resizeEvent(QResizeEvent *evt) override;
private:
    bool ssMode = false;
};

#endif // BLOCKNETADDRESSBOOK_H
