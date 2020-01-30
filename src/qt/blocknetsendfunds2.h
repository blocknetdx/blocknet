// Copyright (c) 2018-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETSENDFUNDS2_H
#define BLOCKNET_QT_BLOCKNETSENDFUNDS2_H

#include <qt/blocknetcoincontrol.h>
#include <qt/blocknetformbtn.h>
#include <qt/blocknetlineedit.h>
#include <qt/blocknetsendfundsutil.h>

#include <qt/walletmodel.h>

#include <QGridLayout>
#include <QFrame>
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
    void setAmount(const CAmount &amt);
    static const int rowHeight = 50;
    static const int columns = 4;

Q_SIGNALS:
    void amount(QString addr, QString amount);
    void remove(QString addr);

private Q_SLOTS:
    void onRemove();

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

private Q_SLOTS:
    void onCoinControl();
    void onChangeAddress();
    void ccAccepted();
    void onSplitChanged();
    void onAmount(QString addr, QString amount);
    void onRemove(QString addr);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void hideEvent(QHideEvent *qHideEvent) override;

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
    QCheckBox *ccSplitOutputCb;
    BlocknetLineEdit *ccSplitOutputTi;
    QLabel *ccSummary2Lbl;
    BlocknetSendFunds2List *bFundList = nullptr;
    BlocknetCoinControlDialog *ccDialog;
    const uint maxSplitOutputs = 1500;

    void updateCoinControl();
    void updateCoinControlSummary();
    uint splitCount(bool *ok = nullptr);
    void updateDisplayUnit();
};

#endif // BLOCKNET_QT_BLOCKNETSENDFUNDS2_H
