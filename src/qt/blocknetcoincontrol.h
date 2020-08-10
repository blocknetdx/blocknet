// Copyright (c) 2018-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETCOINCONTROL_H
#define BLOCKNET_QT_BLOCKNETCOINCONTROL_H

#include <qt/blocknetguiutil.h>
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
#include <QRadioButton>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

class BlocknetCoinControl : public QFrame {
    Q_OBJECT
public:
    explicit BlocknetCoinControl(QWidget *parent = nullptr, WalletModel *w = nullptr);

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
        static std::string key(QString txhash, uint n) {
            return QString("%1-%2").arg(txhash, QString::number(n)).toStdString();
        }
        std::string toString() {
            return key(transaction, vout);
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
        tree->blockSignals(true);
        if (dataModel != nullptr)
            tree->clear();
        tree->blockSignals(false);
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
    void onTreeItemChanged(QTreeWidgetItem *item);

private:
    class BlocknetTableWidget : public QTableWidget {
    public:
        QModelIndex itemIndex(QTableWidgetItem *item) {
            return indexFromItem(item);
        }
    };

private:
    WalletModel *walletModel = nullptr;
    QVBoxLayout *layout;
    BlocknetTableWidget *table;
    QTreeWidget *tree;
    QRadioButton *listRb;
    QRadioButton *treeRb;
    QMenu *contextMenu;
    QTableWidgetItem *contextItem = nullptr;
    QTreeWidgetItem *contextItemTr = nullptr;
    QAction *selectCoins;
    QAction *deselectCoins;
    QAction *selectAllCoins;
    QAction *deselectAllCoins;
    QAction *copyAmountAction;
    QAction *copyLabelAction;
    QAction *copyAddressAction;
    QAction *copyTransactionAction;
    QAction *lockAction;
    QAction *unlockAction;
    QAction *expandAll;
    QAction *collapseAll;

    ModelPtr dataModel = nullptr;

    void setClipboard(const QString &str);
    void unwatch();
    void watch();
    bool utxoForHash(QString transaction, uint vout, UTXO *&utxo);
    QString getTransactionHash(QTableWidgetItem *item);
    QString getTransactionHash(QTreeWidgetItem *item);
    uint getVOut(QTableWidgetItem *item);
    uint getVOut(QTreeWidgetItem *item);
    bool treeMode();
    void showTree(bool yes);

    UTXO* getTableUtxo(QTableWidgetItem *item, int row);
    UTXO* getTreeUtxo(QTreeWidgetItem *item);
    void updateTableUtxos(const QMap<std::string, UTXO*> & utxos);
    void updateTreeUtxos(const QMap<std::string, UTXO*> & utxos);
    QMap<std::string, UTXO*> updateTableCheckStates(const QList<QModelIndex> & idxs, Qt::CheckState checkState, const bool *lockState=nullptr);
    QMap<std::string, UTXO*> updateTreeCheckStates(const QList<QTreeWidgetItem*> & items, Qt::CheckState checkState, const bool *lockState=nullptr);
    QList<QTreeWidgetItem*> allTreeItems();

private:
    bool eventFilter(QObject *obj, QEvent *event) override;

    enum {
        COLUMN_PADDING1,
        COLUMN_CHECKBOX,
        COLUMN_PADDING2,
        COLUMN_AMOUNT,
        COLUMN_PADDING3,
        COLUMN_LABEL,
        COLUMN_PADDING4,
        COLUMN_ADDRESS,
        COLUMN_PADDING5,
        COLUMN_DATE,
        COLUMN_PADDING6,
        COLUMN_CONFIRMATIONS,
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
    class LabelItem : public QTableWidgetItem {
    public:
        explicit LabelItem() = default;
        bool operator < (const QTableWidgetItem & other) const override {
            auto label = data(Qt::DisplayRole).toString();
            auto otherlabel = other.data(Qt::DisplayRole).toString();
            if (label.contains("(") && otherlabel.contains("("))
                return label.toStdString() < otherlabel.toStdString();
            if (label.contains("("))
                return false;
            if (otherlabel.contains("("))
                return true;
            return label.toStdString() < otherlabel.toStdString();
        };
    };

    class TreeWidgetItem : public QTreeWidgetItem {
    public:
        explicit TreeWidgetItem(QTreeWidgetItem *parent = nullptr) : QTreeWidgetItem(parent) { }
        bool operator<(const QTreeWidgetItem & other) const {
            int column = treeWidget()->sortColumn();

            if (column == COLUMN_LABEL) {
                auto label = data(column, Qt::DisplayRole).toString();
                auto otherlabel = other.data(column, Qt::DisplayRole).toString();
                if (label.contains("(") && otherlabel.contains("("))
                    return label.toStdString() < otherlabel.toStdString();
                if (label.contains("("))
                    return false;
                if (otherlabel.contains("("))
                    return true;
                return label.toStdString() < otherlabel.toStdString();
            }

            if (column == BlocknetCoinControl::COLUMN_AMOUNT || column == BlocknetCoinControl::COLUMN_CONFIRMATIONS)
                return data(column, Qt::UserRole).toLongLong() < other.data(column, Qt::UserRole).toLongLong();
            else if (column == BlocknetCoinControl::COLUMN_DATE)
                return data(column, Qt::UserRole).toDateTime().toMSecsSinceEpoch() < other.data(column, Qt::UserRole).toDateTime().toMSecsSinceEpoch();

            return QTreeWidgetItem::operator<(other);
        }
        int64_t camount{0};
    };

    class TreeDelegate : public QStyledItemDelegate {
    public:
        QSize sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const override {
            auto s = QStyledItemDelegate::sizeHint(option, index);
            return { s.width(), BGU::spi(25) };
        }
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
    void setPayAmount(CAmount amount) {
        this->payAmount = amount;
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

#endif // BLOCKNET_QT_BLOCKNETCOINCONTROL_H
