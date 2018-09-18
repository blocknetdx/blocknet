// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETADDRESSEDIT_H
#define BLOCKNETADDRESSEDIT_H

#include "blocknetformbtn.h"
#include "blocknetlineeditwithtitle.h"

#include "walletmodel.h"

#include <QFrame>
#include <QVBoxLayout>
#include <QLabel>
#include <QRadioButton>

class BlocknetAddressEdit : public QFrame {
    Q_OBJECT
public:
    explicit BlocknetAddressEdit(WalletModel *w, QString title = "Edit Address", QString buttonString = "Apply", QFrame *parent = nullptr);

    bool validated();

signals:
    void next();

public slots:
    void clear();
    void addressChanged();
    void createAddressChanged();
    void aliasChanged();
    void onApply() { emit next(); }
protected:
    void keyPressEvent(QKeyEvent *event);
    void focusInEvent(QFocusEvent *event);

private:
    WalletModel *walletModel;
    QString title;
    QString buttonString;
    QVBoxLayout *layout;
    QLabel *addressLbl;
    QLabel *titleLbl;
    BlocknetLineEditWithTitle *addressTi;
    BlocknetLineEditWithTitle *createAddressTi;
    BlocknetLineEditWithTitle *aliasTi;
    QRadioButton *myAddressBtn;
    QRadioButton *otherUserBtn;
    BlocknetFormBtn *confirmBtn;
    BlocknetFormBtn *cancelBtn;
};

#endif // BLOCKNETADDRESSEDIT_H