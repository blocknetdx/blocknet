// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETADDRESSEDIT_H
#define BLOCKNETADDRESSEDIT_H

#include "blocknetformbtn.h"
#include "blocknetlineeditwithtitle.h"

#include "base58.h"
#include "walletmodel.h"
#include "addresstablemodel.h"

#include <QFrame>
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QDialog>

class BlocknetAddressEdit : public QFrame {
    Q_OBJECT
public:
    explicit BlocknetAddressEdit(bool editMode, const QString &title, const QString &buttonString, QWidget *parent = nullptr);
    QSize sizeHint() const override;
    bool validated();
    void setData(const QString &address, const QString &alias, const int &type, const QString &key);
    QString getAddress();
    QString getAlias();
    QString getKey();
    QString getType();
    CKeyID getKeyID();

signals:
    void cancel();
    void accept();

public slots:
    void clear();
    void onApply();
    void onCancel() { emit cancel(); }

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;

private slots:
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
    CKeyID keyID;

    bool generateAddress(QString &newAddress);
};

class BlocknetAddressEditDialog : public QDialog {
    Q_OBJECT
public:
    explicit BlocknetAddressEditDialog(AddressTableModel *model, Qt::WindowFlags f, QWidget *parent = nullptr);
    void accept() override;
    void setData(const QString &address, const QString &alias, const int &type, const QString &key);
    BlocknetAddressEdit *form;
protected:
    void resizeEvent(QResizeEvent *evt) override;
private:
    AddressTableModel *model;
};

class BlocknetAddressAddDialog : public QDialog {
    Q_OBJECT
public:
    explicit BlocknetAddressAddDialog(AddressTableModel *model, WalletModel *walletModel, Qt::WindowFlags f, QWidget *parent = nullptr);
    void accept() override;
    BlocknetAddressEdit *form;
protected:
    void resizeEvent(QResizeEvent *evt) override;
private:
    AddressTableModel *model;
    WalletModel *walletModel;
};

#endif // BLOCKNETADDRESSEDIT_H