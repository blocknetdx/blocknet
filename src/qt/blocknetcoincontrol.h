// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETCOINCONTROL_H
#define BLOCKNETCOINCONTROL_H

#include <qt/blocknetformbtn.h>
#include <qt/blocknetsendfundsutil.h>

#include <qt/walletmodel.h>

#include <memory>

#include <QDateTime>
#include <QDialog>
#include <QGridLayout>
#include <QFrame>
#include <QLabel>
#include <QMenu>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

class BlocknetCoinControl : public QFrame {
    Q_OBJECT
public:
    explicit BlocknetCoinControl(QWidget *parent = nullptr);

    struct UTXO {
        bool checked;
        QString amount;
        QString label;
        QString address;
        QDateTime date;
        int64_t confirmations;
        double priority;
        QString transaction;
        uint vout;
        bool locked;
        bool unlocked;
        int64_t camount;
        bool isValid() {
            return !transaction.isEmpty();
        }
    };
    struct Model {
        double freeThreshold;
        double mempoolPriority;
        QVector<UTXO*> data;
        void copy(Model &m) {
            freeThreshold = m.freeThreshold;
            mempoolPriority = m.mempoolPriority;
            data = m.data;
        }
        void copy(Model *m) {
            copy(*m);
        }
    };
    typedef std::shared_ptr<Model> ModelPtr;

    void setData(ModelPtr dataModel);
    ModelPtr getData();

    void clear() {
        table->blockSignals(true);
        if (dataModel != nullptr)
            table->clearContents();
        table->blockSignals(false);
    };

    QTableWidget* getTable() {
        return table;
    }

    void sizeTo(int minimumHeight, int maximumHeight);
    QString getPriorityLabel(double dPriority);

Q_SIGNALS:
    void tableUpdated();

public Q_SLOTS:

private Q_SLOTS:
    void showContextMenu(QPoint);
    void onItemChanged(QTableWidgetItem *item);

private:
    QVBoxLayout *layout;
    QTableWidget *table;
    QMenu *contextMenu;
    QTableWidgetItem *contextItem = nullptr;
    QAction *selectCoins;
    QAction *deselectCoins;

    ModelPtr dataModel = nullptr;

    void setClipboard(const QString &str);
    void unwatch();
    void watch();
    bool utxoForHash(QString transaction, uint vout, UTXO *&utxo);
    QString getTransactionHash(QTableWidgetItem *item);
    uint getVOut(QTableWidgetItem *item);

    enum {
        COLUMN_PADDING,
        COLUMN_CHECKBOX,
        COLUMN_AMOUNT,
        COLUMN_LABEL,
        COLUMN_ADDRESS,
        COLUMN_DATE,
        COLUMN_CONFIRMATIONS,
        COLUMN_PRIORITY,
        COLUMN_TXHASH,
        COLUMN_TXVOUT,
    };

    class NumberItem : public QTableWidgetItem {
    public:
        explicit NumberItem() = default;
        bool operator < (const QTableWidgetItem &other) const override {
            auto i1 = text().toDouble();
            auto i2 = other.text().toDouble();
            return i1 < i2;
        };
    };
    class PriorityItem : public QTableWidgetItem {
    public:
        explicit PriorityItem() = default;
        enum {
            PriorityRole = 200
        };
        bool operator < (const QTableWidgetItem &other) const override {
            return data(PriorityRole).toDouble() < other.data(PriorityRole).toDouble();
        };
    };
};

class BlocknetCoinControlDialog : public QDialog {
    Q_OBJECT
public:
    explicit BlocknetCoinControlDialog(WalletModel *w, QWidget *parent = nullptr, Qt::WindowFlags f = 0, bool standaloneMode = false);
    void resizeEvent(QResizeEvent *evt) override;
    void clear() {
        payAmount = 0;
        cc->clear();
        updateLabels();
    }
    BlocknetCoinControl* getCC() { return cc; }
    void setPayAmount(CAmount payAmount) {
        this->payAmount = payAmount;
    }

    void populateUnspentTransactions(const QVector<BlocknetSimpleUTXO> & txSelectedUtxos);
    
public Q_SLOTS:
    void updateUTXOState();

protected:
    void showEvent(QShowEvent *event) override;

private:
    WalletModel *walletModel;
    QFrame *content;
    BlocknetFormBtn *confirmBtn;
    BlocknetFormBtn *cancelBtn;
    BlocknetCoinControl *cc;
    QFrame *feePanel;
    QGridLayout *feePanelLayout;
    QLabel *quantityLbl;
    QLabel *quantityVal;
    QLabel *amountLbl;
    QLabel *amountVal;
    QLabel *feeLbl;
    QLabel *feeVal;
    QLabel *afterFeeLbl;
    QLabel *afterFeeVal;
    QLabel *bytesLbl;
    QLabel *bytesVal;
    QLabel *priorityLbl;
    QLabel *priorityVal;
    QLabel *dustLbl;
    QLabel *dustVal;
    QLabel *changeLbl;
    QLabel *changeVal;
    CAmount payAmount;
    bool standaloneMode;
    void updateLabels();
};

#endif // BLOCKNETCOINCONTROL_H
