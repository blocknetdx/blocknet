// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETADDRESSEDIT_H
#define BLOCKNET_QT_BLOCKNETADDRESSEDIT_H

#include <qt/blocknetformbtn.h>
#include <qt/blocknetlineeditwithtitle.h>

#include <qt/walletmodel.h>
#include <qt/addresstablemodel.h>

#include <base58.h>

#include <QDialog>
#include <QFrame>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QWidget>

class BlocknetAddressEdit : public QFrame {
    Q_OBJECT
public:
    explicit BlocknetAddressEdit(bool editMode, const QString &title, const QString &buttonString, QWidget *parent = nullptr);
    QSize sizeHint() const override;
    bool validated();
    bool setNewAddress(const CTxDestination & dest, const CPubKey & pkey);
    void setData(const QString &address, const QString &alias, const int &type, const QString &key);
    QString getAddress();
    QString getAlias();
    QString getKey();
    QString getType();
    CPubKey getPubKey();

Q_SIGNALS:
    void cancel();
    void accept();

public Q_SLOTS:
    void clear();
    void onApply();
    void onCancel() { Q_EMIT cancel(); }

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;

private Q_SLOTS:
    void onPrivateKey(const QString &text);
    void onAddressChanged(const QString &text);
    void onOtherUser(bool checked);

private:
    QVBoxLayout *layout;
    QString title;
    QString buttonString;
    bool editMode;
    QLabel *titleLbl;
    BlocknetLineEditWithTitle *addressTi;
    BlocknetLineEditWithTitle *createAddressTi;
    BlocknetLineEditWithTitle *aliasTi;
    QRadioButton *myAddressBtn;
    QRadioButton *otherUserBtn;
    BlocknetFormBtn *confirmBtn;
    BlocknetFormBtn *cancelBtn;
    CPubKey pubkey;
};

class BlocknetAddressEditDialog : public QDialog {
    Q_OBJECT
public:
    explicit BlocknetAddressEditDialog(AddressTableModel *model, WalletModel *walletModel, Qt::WindowFlags f, QWidget *parent = nullptr);
    void accept() override;
    void setData(const QString &address, const QString &alias, const int &type, const QString &key);
    BlocknetAddressEdit *form;
protected:
    void resizeEvent(QResizeEvent *evt) override;
private:
    AddressTableModel *model;
    WalletModel *walletModel;
};

class BlocknetAddressAddDialog : public QDialog {
Q_OBJECT
public:
    explicit BlocknetAddressAddDialog(AddressTableModel *model, WalletModel *walletModel, Qt::WindowFlags f, QWidget *parent = nullptr);
    void accept() override;
    BlocknetAddressEdit *form;
Q_SIGNALS:
    void rescan(const std::string &);
protected:
    void resizeEvent(QResizeEvent *evt) override;
    bool importPrivateKey(CKey & key, const QString & alias, const QString & addr);
private:
    AddressTableModel *model;
    WalletModel *walletModel;
};

#endif // BLOCKNET_QT_BLOCKNETADDRESSEDIT_H