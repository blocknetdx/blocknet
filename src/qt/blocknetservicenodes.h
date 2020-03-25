// Copyright (c) 2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETSERVICENODES_H
#define BLOCKNET_QT_BLOCKNETSERVICENODES_H

#include <qt/blocknetdropdown.h>
#include <qt/blocknetvars.h>

#include <qt/clientmodel.h>

#include <servicenode/servicenode.h>
#include <validation.h>

#include <QDateTime>
#include <QDialog>
#include <QFrame>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>

class BlocknetServiceNodes : public QFrame {
    Q_OBJECT

public:
    explicit BlocknetServiceNodes(QFrame *parent = nullptr);
    void setClientModel(ClientModel *c);

    void clear() {
        if (dataModel.count() > 0)
            table->clearContents();
    };

    QTableWidget* getTable() {
        return table;
    }

Q_SIGNALS:
    void tableUpdated();

private Q_SLOTS:
    void onItemChanged(QTableWidgetItem *item);
    void onFilter();
    void showServiceNodeDetails(const sn::ServiceNode & snode);
    void setNumBlocks(int count, const QDateTime & blockDate, double nVerificationProgress, bool header);

private:
    QVBoxLayout *layout;
    ClientModel *clientModel;
    QLabel *titleLbl;
    QLabel *filterLbl;
    QTableWidget *table;
    QMenu *contextMenu;
    QTableWidgetItem *contextItem = nullptr;
    BlocknetDropdown *filterDd;
    QVector<sn::ServiceNode> dataModel;
    QVector<sn::ServiceNode> filteredData;
    QTimer *timer;
    int lastRow = -1;
    qint64 lastSelection = 0;
    int lastKnownSnodeCount{0};

    void initialize();
    void setData(const QVector<sn::ServiceNode> & data);
    QVector<sn::ServiceNode> filtered(const int & filter);
    void unwatch();
    void watch();
    void refresh(bool force = false);
    void showContextMenu(QPoint pt);
    sn::ServiceNode serviceNodeForPubkey(const std::string & hex);

    enum {
        COLUMN_PRIVKEY, // hidden
        COLUMN_COLOR,
        COLUMN_ALIAS,
        COLUMN_IP,
        COLUMN_STATUS,
        COLUMN_LASTSEEN,
        COLUMN_PUBKEY,
        COLUMN_SERVICES,
        COLUMN_PADDING,
    };

    enum {
        FILTER_ALL,
        FILTER_MINE,
        FILTER_OTHER,
        FILTER_RUNNING,
        FILTER_OFFLINE,
    };

};

class BlocknetServiceNodeDetailsDialog : public QDialog {
Q_OBJECT
public:
    explicit BlocknetServiceNodeDetailsDialog(const sn::ServiceNode & snode, QWidget *parent = nullptr);

protected:

private:
};

#endif // BLOCKNET_QT_BLOCKNETSERVICENODES_H
