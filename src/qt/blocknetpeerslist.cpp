// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetpeerslist.h"

#include <QMessageBox>
#include <QHeaderView>

BlocknetPeersList::BlocknetPeersList(QWidget *popup, int id, QFrame *parent) : BlocknetToolsPage(id, parent), popupWidget(popup), layout(new QVBoxLayout) {
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(layout);
    layout->setContentsMargins(0, 10, 0, 10);

    titleLbl = new QLabel(tr("Peers List"));
    titleLbl->setObjectName("h2");

    table = new QTableWidget;
    table->setContentsMargins(QMargins());
    table->setColumnCount(COLUMN_LATEST_BLOCK + 1);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setFocusPolicy(Qt::NoFocus);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
    table->setFocusPolicy(Qt::NoFocus);
    table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->setContextMenuPolicy(Qt::CustomContextMenu);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->verticalHeader()->setDefaultSectionSize(78);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    table->horizontalHeader()->setSortIndicatorShown(true);
    table->horizontalHeader()->setSectionsClickable(true);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_ADDRESS, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_VERSION, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_PING_TIME, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_BAN_SCORE, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_LATEST_BLOCK, QHeaderView::ResizeToContents);
    table->setHorizontalHeaderLabels({ tr("Address"), tr("Version"), tr("Ping Time"), tr("Ban Score"), tr("Latest Block") });

    layout->addWidget(titleLbl, 0, Qt::AlignTop | Qt::AlignLeft);
    layout->addSpacing(5);
    layout->addWidget(table);
    layout->addSpacing(5);

    peerDetails = new BlocknetPeerDetails;
    peerDetails->setDisplayWidget(popupWidget);
    peerDetails->hide();

    connect(table->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)), this,
            SLOT(displayPeerDetails(const QItemSelection &, const QItemSelection &)));
}

void BlocknetPeersList::setWalletModel(WalletModel *w) {
    if (!walletModel)
        return;

    initialize();
}

void BlocknetPeersList::initialize() {
    if (!walletModel)
        return;

    dataModel.clear();

    Peer peerData1 = {
        tr("23.567.93.444:00987"),
        tr("/BlocknetDX Core:3.7.54/"),
        tr("16ms"),
        tr("00000"),
        tr("0000000000")
    };

    Peer peerData2 = {
        tr("23.567.93.444:00987"),
        tr("/BlocknetDX Core:3.7.54/"),
        tr("16ms"),
        tr("00000"),
        tr("0000000000")
    };

    Peer peerData3 = {
        tr("23.567.93.444:00987"),
        tr("/BlocknetDX Core:3.7.54/"),
        tr("16ms"),
        tr("00000"),
        tr("0000000000")
    };

    Peer peerData4 = {
        tr("23.567.93.444:00987"),
        tr("/BlocknetDX Core:3.7.54/"),
        tr("16ms"),
        tr("00000"),
        tr("0000000000")
    };

    Peer peerData5 = {
        tr("23.567.93.444:00987"),
        tr("/BlocknetDX Core:3.7.54/"),
        tr("16ms"),
        tr("00000"),
        tr("0000000000")
    };

    dataModel << peerData1 << peerData2 << peerData3 << peerData4 << peerData5;

    // Sort on address descending
    std::sort(dataModel.begin(), dataModel.end(), [](const Peer &a, const Peer &b) {
        return a.address > b.address;
    });

    this->setData(dataModel);
}

void BlocknetPeersList::setData(QVector<Peer> data) {
    unwatch();
    table->clearContents();
    table->setRowCount(data.count());
    table->setSortingEnabled(false);

    for (int i = 0; i < data.count(); ++i) {
        auto &d = data[i];

        // address
        auto *addressItem = new QTableWidgetItem;
        addressItem->setData(Qt::DisplayRole, d.address);
        table->setItem(i, COLUMN_ADDRESS, addressItem);

        // version
        auto *versionItem = new QTableWidgetItem;
        versionItem->setData(Qt::DisplayRole, d.version);
        table->setItem(i, COLUMN_VERSION, versionItem);

        // ping time
        auto *pingItem = new QTableWidgetItem;
        pingItem->setData(Qt::DisplayRole, d.pingTime);
        table->setItem(i, COLUMN_PING_TIME, pingItem);

        // ban score
        auto *scoreItem = new QTableWidgetItem;
        scoreItem->setData(Qt::DisplayRole, d.banScore);
        table->setItem(i, COLUMN_BAN_SCORE, scoreItem);

        // latest block
        auto *latestItem = new QTableWidgetItem;
        latestItem->setData(Qt::DisplayRole, d.latestBlock);
        table->setItem(i, COLUMN_LATEST_BLOCK, latestItem);
    }

    table->setSortingEnabled(true);
    watch();
}

void BlocknetPeersList::unwatch() {
    table->setEnabled(false);
    //disconnect(table, &QTableWidget::itemChanged, this, &BlocknetPeersList::onItemChanged);
}

void BlocknetPeersList::watch() {
    table->setEnabled(true);
    //connect(table, &QTableWidget::itemChanged, this, &BlocknetPeersList::onItemChanged);
}

void BlocknetPeersList::displayPeerDetails(const QItemSelection &, const QItemSelection &) {
    if (!table->selectionModel()) {
        return;
    }

    if (peerDetails->isHidden()) {
        peerDetails->setFixedSize(table->width(), 250);
        peerDetails->move(QPoint(table->pos().x() + 46, table->pos().y() + table->height() - peerDetails->height() / 2));
        BlocknetPeerDetails::PeerDetails detailsData = {
            tr("N/A"),
            tr("N/A"),
            tr("N/A"),
            tr("N/A"),
            tr("N/A"),
            tr("N/A"),
            tr("N/A"),
            tr("N/A"),
            tr("N/A"),
            tr("N/A"),
            tr("N/A"),
            tr("N/A")
        };
        peerDetails->show(detailsData);
    }
}
