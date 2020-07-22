// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETADDRESSBOOK_H
#define BLOCKNET_QT_BLOCKNETADDRESSBOOK_H

#include <qt/blocknetactionbtn.h>
#include <qt/blocknetdropdown.h>
#include <qt/blocknetfundsmenu.h>

#include <qt/walletmodel.h>

#include <QDialog>
#include <QFrame>
#include <QLabel>
#include <QResizeEvent>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

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
        bool operator<(const Address & other) const {
            return address.toStdString() < other.address.toStdString();
        }
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
        COLUMN_PADDING1,
        COLUMN_ALIAS,
        COLUMN_PADDING2,
        COLUMN_ADDRESS,
        COLUMN_PADDING3,
        COLUMN_COPY,
        COLUMN_EDIT,
        COLUMN_DELETE,
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

public Q_SLOTS:
    void onAddressAction();

Q_SIGNALS:
    void send(const QString &);
    void rescan(const std::string &);

private Q_SLOTS:
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
Q_SIGNALS:
    void send(const QString &);
protected:
    void resizeEvent(QResizeEvent *evt) override;
private:
    bool ssMode = false;
};

#endif // BLOCKNET_QT_BLOCKNETADDRESSBOOK_H
