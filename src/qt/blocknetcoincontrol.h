// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETCOINCONTROL_H
#define BLOCKNETCOINCONTROL_H

#include "blocknetformbtn.h"

#include <memory>

#include <QFrame>
#include <QVBoxLayout>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QMenu>
#include <QDateTime>
#include <QVector>
#include <QWidget>
#include <QDialog>
#include <QLabel>

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

signals:
    void tableUpdated();

public slots:

private slots:
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
    QString getPriorityLabel(double dPriority);
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
    explicit BlocknetCoinControlDialog(QWidget *parent = nullptr);
    void resizeEvent(QResizeEvent *evt) override;
    void clear() {
        totalValueLbl->clear();
        cc->clear();
    }
    BlocknetCoinControl* getCC() { return cc; }
private:
    QFrame *content;
    BlocknetFormBtn *confirmBtn;
    BlocknetFormBtn *cancelBtn;
    BlocknetCoinControl *cc;
    QLabel *totalValueLbl;
};

#endif // BLOCKNETCOINCONTROL_H
