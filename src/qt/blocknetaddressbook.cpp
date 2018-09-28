// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetaddressbook.h"
#include "blockneticonaltbtn.h"
#include "blocknetlabelbtn.h"
#include "blocknetavatar.h"
#include "addresstablemodel.h"

#include <QHeaderView>

BlocknetAddressBook::BlocknetAddressBook(QWidget *popup, QFrame *parent) : QFrame(parent), popupWidget(popup), layout(new QVBoxLayout) {
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->setContentsMargins(46, 10, 50, 0);
    this->setLayout(layout);

    titleLbl = new QLabel(tr("Address Book"));
    titleLbl->setObjectName("h4");
    titleLbl->setFixedHeight(26);

    auto *topBox = new QFrame;
    auto *topBoxLayout = new QHBoxLayout;
    topBoxLayout->setContentsMargins(QMargins());
    topBox->setLayout(topBoxLayout);

    auto *addAddressBtn = new BlocknetIconAltBtn(":/redesign/QuickActions/AddressButtonIcon.png", 5);

    addButtonLbl = new QLabel(tr("Add Address"));
    addButtonLbl->setObjectName("h4");

    filterLbl = new QLabel(tr("Filter by:"));
    filterLbl->setObjectName("title");

    QStringList list{tr("All Addresses"), tr("Sending"), tr("Receiving")};
    addressDropdown = new BlocknetDropdown(list);

    table = new QTableWidget;
    table->setContentsMargins(QMargins());
    table->setColumnCount(COLUMN_DELETE + 1);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setFocusPolicy(Qt::NoFocus);
    table->setAlternatingRowColors(true);
    table->setColumnWidth(COLUMN_ACTION, 50);
    table->setColumnWidth(COLUMN_AVATAR, 50);
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
    table->horizontalHeader()->setSectionResizeMode(COLUMN_ACTION, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_AVATAR, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_ALIAS, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_ADDRESS, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_COPY, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_EDIT, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_DELETE, QHeaderView::Stretch);
    table->setHorizontalHeaderLabels({ "", "", tr("Alias"), tr("Address"), "", "", "" });

    topBoxLayout->addWidget(addAddressBtn, Qt::AlignLeft);
    topBoxLayout->addWidget(addButtonLbl, Qt::AlignLeft);
    topBoxLayout->addStretch(1);
    topBoxLayout->addWidget(filterLbl);
    topBoxLayout->addWidget(addressDropdown);

    layout->addWidget(titleLbl);
    layout->addSpacing(10);
    layout->addWidget(topBox);
    layout->addSpacing(15);
    layout->addWidget(table);
    layout->addSpacing(20);

    fundsMenu = new BlocknetFundsMenu;
    fundsMenu->setDisplayWidget(popupWidget);
    fundsMenu->hOnSendFunds = [&]() { emit sendFunds(); };
    fundsMenu->hOnRequestFunds = [&]() { emit requestFunds(); };
    fundsMenu->hide();
}

void BlocknetAddressBook::setWalletModel(WalletModel *w) {
    if (walletModel == w)
        return;

    walletModel = w;
    if (!walletModel || !walletModel->getOptionsModel())
        return;

    initialize();
}

void BlocknetAddressBook::initialize() {
    if (!walletModel)
        return;

    dataModel.clear();

    AddressTableModel *addressTableModel = walletModel->getAddressTableModel();
    int rowCount = addressTableModel->rowCount(QModelIndex());
    int columnCount = addressTableModel->columnCount(QModelIndex());

    for (int row=0; row<rowCount; row++) {
        QString alias = "";
        QString address = "";
        for (int column=0; column<columnCount; column++) {
            QModelIndex index = addressTableModel->index(row, column, QModelIndex());
            QVariant variant = addressTableModel->data(index, Qt::DisplayRole);
            if (column == 0) {
                alias = variant.toString();
            } else {
                address = variant.toString();
            }
        }
        Address a = {
            alias,
            address
        };
        dataModel << a;
    }

    // Sort on alias descending
    std::sort(dataModel.begin(), dataModel.end(), [](const Address &a, const Address &b) {
        return a.alias > b.alias;
    });

    this->setData(dataModel);
}

void BlocknetAddressBook::unwatch() {
    table->setEnabled(false);
}

void BlocknetAddressBook::watch() {
    table->setEnabled(true);
}

QVector<BlocknetAddressBook::Address> BlocknetAddressBook::filtered(int filter, int chainHeight) {
    QVector<Address> r;
    for (auto &d : dataModel) {
        switch (filter) {
            case FILTER_SENDING: 
            case FILTER_RECEIVING: 
            case FILTER_ALL:
            default:
                r.push_back(d);
                break;
        }
    }
    return r;
}

void BlocknetAddressBook::setData(QVector<Address> data) {
    this->filteredData = data;

    unwatch();
    table->clearContents();
    table->setRowCount(this->filteredData.count());
    table->setSortingEnabled(false);

    for (int i = 0; i < this->filteredData.count(); ++i) {
        auto &d = this->filteredData[i];

        // action item
        auto *actionItem = new QTableWidgetItem;
        auto *widget = new QWidget();
        widget->setContentsMargins(QMargins());
        auto *boxLayout = new QVBoxLayout;
        boxLayout->setContentsMargins(QMargins());
        boxLayout->setSpacing(0);
        widget->setLayout(boxLayout);

        auto *button = new BlocknetActionBtn;
        button->setID(d.address);
        boxLayout->addWidget(button, 0, Qt::AlignCenter);
        connect(button, &BlocknetActionBtn::clicked, this, &BlocknetAddressBook::onAddressAction);

        table->setCellWidget(i, COLUMN_ACTION, widget);
        table->setItem(i, COLUMN_ACTION, actionItem);

        // avatar
        auto *avatarItem = new QTableWidgetItem;
        auto *avatarWidget = new QWidget();
        avatarWidget->setContentsMargins(QMargins());
        auto *avatarLayout = new QVBoxLayout;
        avatarLayout->setContentsMargins(QMargins());
        avatarLayout->setSpacing(0);
        avatarWidget->setLayout(avatarLayout);

        auto *avatar = new BlocknetAvatar(d.alias);
        avatarLayout->addWidget(avatar, 0, Qt::AlignCenter);
        
        table->setCellWidget(i, COLUMN_AVATAR, avatarWidget);
        table->setItem(i, COLUMN_AVATAR, avatarItem);

        // alias
        auto *aliasItem = new QTableWidgetItem;
        aliasItem->setData(Qt::DisplayRole, d.alias);
        table->setItem(i, COLUMN_ALIAS, aliasItem);

        // address
        auto *addressItem = new QTableWidgetItem;
        addressItem->setData(Qt::DisplayRole, d.address);
        table->setItem(i, COLUMN_ADDRESS, addressItem);

        // copy item
        auto *copyItem = new QTableWidgetItem;
        auto *copyWidget = new QWidget();
        copyWidget->setContentsMargins(QMargins());
        auto *copyLayout = new QVBoxLayout;
        copyLayout->setContentsMargins(QMargins());
        copyLayout->setSpacing(0);
        copyWidget->setLayout(copyLayout);

        auto *copyButton = new BlocknetLabelBtn;
        copyButton->setText(tr("Copy Address"));
        //copyButton->setFixedSize(40, 40);
        copyButton->setID(d.address);
        copyLayout->addWidget(copyButton, 0, Qt::AlignCenter);
        copyLayout->addSpacing(6);
        //connect(copyButton, &BlocknetLabelBtn::clicked, this, &BlocknetAddressBook::onAddressAction);

        table->setCellWidget(i, COLUMN_COPY, copyWidget);
        table->setItem(i, COLUMN_COPY, copyItem);

        // edit item
        auto *editItem = new QTableWidgetItem;
        auto *editWidget = new QWidget();
        editWidget->setContentsMargins(QMargins());
        auto *editLayout = new QVBoxLayout;
        editLayout->setContentsMargins(QMargins());
        editLayout->setSpacing(0);
        editWidget->setLayout(editLayout);

        auto *editButton = new BlocknetLabelBtn;
        editButton->setText(tr("Edit"));
        //editButton->setFixedSize(40, 40);
        editButton->setID(d.address);
        editLayout->addWidget(editButton, 0, Qt::AlignCenter);
        editLayout->addSpacing(6);
        //connect(editButton, &BlocknetLabelBtn::clicked, this, &BlocknetAddressBook::onAddressAction);

        table->setCellWidget(i, COLUMN_EDIT, editWidget);
        table->setItem(i, COLUMN_EDIT, editItem);

        // delete item
        auto *deleteItem = new QTableWidgetItem;
        auto *deleteWidget = new QWidget();
        deleteWidget->setContentsMargins(QMargins());
        auto *deleteLayout = new QVBoxLayout;
        deleteLayout->setContentsMargins(QMargins());
        deleteLayout->setSpacing(0);
        deleteWidget->setLayout(deleteLayout);

        auto *deleteButton = new BlocknetLabelBtn;
        deleteButton->setText(tr("Delete"));
        //deleteButton->setFixedSize(40, 40);
        deleteButton->setID(d.address);
        deleteLayout->addWidget(deleteButton, 0, Qt::AlignCenter);
        deleteLayout->addSpacing(6);
        //connect(deleteButton, &BlocknetLabelBtn::clicked, this, &BlocknetAddressBook::onAddressAction);

        table->setCellWidget(i, COLUMN_DELETE, deleteWidget);
        table->setItem(i, COLUMN_DELETE, deleteItem);
    }

    table->setSortingEnabled(true);
    watch();
}

void BlocknetAddressBook::onAddressAction() {
    auto *btn = qobject_cast<BlocknetActionBtn*>(sender());
    auto addressHash = uint256S(btn->getID().toStdString());
    if (fundsMenu->isHidden()) {
        QPoint li = btn->mapToGlobal(QPoint());
        QPoint npos = popupWidget->mapFromGlobal(QPoint(li.x() - 2, li.y() + btn->height() + 2));
        fundsMenu->move(npos);
        fundsMenu->show();
    }
}

