// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETSENDFUNDS2_H
#define BLOCKNETSENDFUNDS2_H

#include "blocknetsendfundsutil.h"
#include "blocknetformbtn.h"
#include "blocknetlineedit.h"

#include "walletmodel.h"

#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QRadioButton>
#include <QScrollArea>

class BlocknetSendFunds2List : public QFrame {
    Q_OBJECT
public:
    explicit BlocknetSendFunds2List(int displayUnit, QFrame *parent = nullptr);
    QSize sizeHint() const override;
    static QString getName() {
        return QString("funds");
    }
    void clear();
    void requestFocus();
    void addRow(int row, QString addr, QString amount);
    static const int rowHeight = 50;
    static const int columns = 4;

signals:
    void amount(QString addr, QString amount);

private slots:
    void onAmount();

private:
    int displayUnit;
    QGridLayout *gridLayout;
    QSet<QWidget*> widgets;
    QVector<BlocknetLineEdit*> tis;
};

class BlocknetSendFunds2 : public BlocknetSendFundsPage {
    Q_OBJECT
public:
    explicit BlocknetSendFunds2(WalletModel *w, int id, QFrame *parent = nullptr);
    void setData(BlocknetSendFundsModel *model) override;
    bool validated() override;
    void clear() override;

private slots:
    void onCoinControl();
    void onAmount(QString addr, QString amount);
    void onChangeAddress();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;

private:
    int displayUnit;

    QVBoxLayout *layout;
    QScrollArea *scrollArea;
    QFrame *content;
    QVBoxLayout *contentLayout;
    QLabel *titleLbl;
    BlocknetLineEdit *changeAddrTi;
    QFrame *ccManualBox;
    QFrame *fundList;
    BlocknetFormBtn *continueBtn;
    BlocknetFormBtn *cancelBtn;
    QRadioButton *ccDefaultRb;
    QRadioButton *ccManualRb;
    BlocknetSendFunds2List *bFundList = nullptr;
};

#endif // BLOCKNETSENDFUNDS2_H
