// Copyright (c) 2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetaccounts.h>

#include <qt/blocknetaddressedit.h>
#include <qt/blocknetavatar.h>
#include <qt/blocknetguiutil.h>
#include <qt/blockneticonbtn.h>
#include <qt/blocknetlabelbtn.h>

#include <qt/addresstablemodel.h>
#include <qt/bitcoinunits.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>

#include <key_io.h>

#include <QHeaderView>
#include <QSettings>
#include <QTimer>

BlocknetAccounts::BlocknetAccounts(QWidget *parent) : QFrame(parent), layout(new QVBoxLayout) {
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->setContentsMargins(BGU::spi(46), BGU::spi(10), BGU::spi(50), 0);
    this->setLayout(layout);

    titleLbl = new QLabel(tr("Balances"));
    titleLbl->setObjectName("h4");
    titleLbl->setFixedHeight(BGU::spi(26));

    auto *topBox = new QFrame;
    topBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
    auto *topBoxLayout = new QHBoxLayout;
    topBoxLayout->setContentsMargins(QMargins());
    topBoxLayout->setSizeConstraint(QLayout::SetMaximumSize);
    topBox->setLayout(topBoxLayout);

    auto *addAddressBtn = new BlocknetIconBtn(":/redesign/QuickActions/AddressButtonIcon.png");

    addButtonLbl = new QLabel(tr("Create New Address"));
    addButtonLbl->setObjectName("h4");

    table = new QTableWidget;
    table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    table->setContentsMargins(QMargins());
    table->setColumnCount(COLUMN_EDIT + 1);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setFocusPolicy(Qt::NoFocus);
    table->setAlternatingRowColors(true);
    table->setColumnWidth(COLUMN_PADDING1, BGU::spi(5));
    table->setColumnWidth(COLUMN_PADDING2, BGU::spi(1));
    table->setColumnWidth(COLUMN_PADDING3, BGU::spi(1));
    table->setColumnWidth(COLUMN_PADDING4, BGU::spi(1));
    table->setColumnWidth(COLUMN_PADDING5, BGU::spi(1));
    table->setColumnWidth(COLUMN_PADDING6, BGU::spi(1));
    table->setColumnWidth(COLUMN_AVATAR, BGU::spi(50));
    table->setColumnWidth(COLUMN_COPY, BGU::spi(65));
    table->setColumnWidth(COLUMN_EDIT, BGU::spi(65));
    table->setShowGrid(false);
    table->setFocusPolicy(Qt::NoFocus);
    table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setContextMenuPolicy(Qt::CustomContextMenu);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->verticalHeader()->setDefaultSectionSize(BGU::spi(58));
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    table->horizontalHeader()->setSortIndicatorShown(true);
    table->horizontalHeader()->setSectionsClickable(true);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_PADDING1, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_PADDING2, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_PADDING3, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_PADDING4, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_PADDING5, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_PADDING6, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_AVATAR, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_ALIAS, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_ADDRESS, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_BALANCE, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_INPUTS, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_COPY, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_EDIT, QHeaderView::Fixed);
    table->setHorizontalHeaderLabels({ "", "", "", tr("Alias"), "", tr("Address"), "", tr("Balance"), "", tr("Inputs"), "", "", "" });

    topBoxLayout->addWidget(addAddressBtn, 0, Qt::AlignLeft);
    topBoxLayout->addWidget(addButtonLbl, 0, Qt::AlignLeft | Qt::AlignVCenter);
    topBoxLayout->addStretch(1);

    layout->addWidget(titleLbl);
    layout->addSpacing(BGU::spi(10));
    layout->addWidget(topBox);
    layout->addSpacing(BGU::spi(15));
    layout->addWidget(table, 1);
    layout->addSpacing(BGU::spi(20));

    connect(addAddressBtn, &BlocknetIconBtn::clicked, this, &BlocknetAccounts::onAddAddress);
    connect(table, &QTableWidget::cellDoubleClicked, this, &BlocknetAccounts::onDoubleClick);
    connect(table->horizontalHeader(), &QHeaderView::sortIndicatorChanged, this, [this](int column, Qt::SortOrder order) {
        QSettings settings;
        if (column <= COLUMN_PADDING2 || column == COLUMN_PADDING3 || column == COLUMN_PADDING4
         || column == COLUMN_PADDING5 || column == COLUMN_PADDING6) {
            table->horizontalHeader()->setSortIndicator(settings.value("blocknetAccountsSortColumn").toInt(),
                    static_cast<Qt::SortOrder>(settings.value("blocknetAccountsSortOrder").toInt()));
            return;
        }
        settings.setValue("blocknetAccountsSortOrder", static_cast<int>(order));
        settings.setValue("blocknetAccountsSortColumn", column);
    });
}

void BlocknetAccounts::setWalletModel(WalletModel *w) {
    if (walletModel == w)
        return;

    walletModel = w;
    if (!walletModel || !walletModel->getOptionsModel())
        return;

    initialize();

    connect(walletModel->getAddressTableModel(), &AddressTableModel::rowsInserted, this,
        [this](const QModelIndex &, int, int) {
            initialize();
        });
    connect(walletModel->getAddressTableModel(), &AddressTableModel::rowsRemoved, this,
        [this](const QModelIndex &, int, int) {
            initialize();
        });
    connect(walletModel->getAddressTableModel(), &AddressTableModel::dataChanged, this,
        [this](const QModelIndex &, const QModelIndex &, const QVector<int> &) {
            initialize();
        });
}

void BlocknetAccounts::initialize() {
    if (!walletModel)
        return;

    dataModel.clear();

    AddressTableModel *addressTableModel = walletModel->getAddressTableModel();
    int rowCount = addressTableModel->rowCount(QModelIndex());

    std::set<BlocknetAddressBook::Address> addresses;
    for (int row = 0; row < rowCount; ++row) {
        auto index = addressTableModel->index(row, 0, QModelIndex());
        auto *rec = static_cast<AddressTableEntry*>(index.internalPointer());
        if (!rec)
            continue;
        QString alias = rec->label;
        QString address = rec->address;
        int type = rec->type;
        BlocknetAddressBook::Address a = {
            alias,
            address,
            type,
        };
        addresses.insert(a);
    }

    // Tally up balances for each address
    auto coins = walletModel->wallet().listCoins();
    for (auto & item : coins) {
        const auto addr = EncodeDestination(item.first);
        auto temp = BlocknetAddressBook::Address{ "", QString::fromStdString(addr), AddressTableEntry::Type::Receiving };
        auto it = addresses.find(temp);
        if (it != addresses.end()) {
            Account account;
            account.alias = it->alias;
            account.type = it->type;
            account.address = QString::fromStdString(EncodeDestination(item.first));
            account.inputs = item.second.size();
            for (const auto & tup : item.second) {
                auto tx = std::get<1>(tup);
                account.balance += tx.txout.nValue;
            }
            dataModel << account;
        }
    }

    this->setData(dataModel);
}

void BlocknetAccounts::unwatch() {
    table->setEnabled(false);
}

void BlocknetAccounts::watch() {
    table->setEnabled(true);
}

void BlocknetAccounts::setData(const QVector<Account> &data) {
    this->filteredData = data;

    unwatch();
    table->clearContents();
    table->setRowCount(this->filteredData.count());
    table->setSortingEnabled(false);

    for (int i = 0; i < this->filteredData.count(); ++i) {
        auto &d = this->filteredData[i];

        // avatar
        auto *avatarItem = new QTableWidgetItem;
        auto *avatarWidget = new QWidget();
        avatarWidget->setContentsMargins(QMargins());
        auto *avatarLayout = new QVBoxLayout;
        avatarLayout->setContentsMargins(QMargins());
        avatarLayout->setSpacing(0);
        avatarWidget->setLayout(avatarLayout);

        auto *avatar = d.type == AddressTableEntry::Sending ? new BlocknetAvatar(d.address)
                                                            : new BlocknetAvatarBlue(d.address);
        avatarLayout->addWidget(avatar, 0, Qt::AlignCenter);
        table->setCellWidget(i, COLUMN_AVATAR, avatarWidget);
        table->setItem(i, COLUMN_AVATAR, avatarItem);

        // alias
        auto *aliasItem = new LabelItem;
        aliasItem->label = d.alias.toStdString();
        aliasItem->setData(Qt::DisplayRole, d.alias);
        table->setItem(i, COLUMN_ALIAS, aliasItem);

        // address
        auto *addressItem = new QTableWidgetItem;
        addressItem->setData(Qt::DisplayRole, d.address);
        table->setItem(i, COLUMN_ADDRESS, addressItem);

        // balance
        auto *balanceItem = new NumberItem;
        balanceItem->amount = d.balance;
        balanceItem->setData(Qt::DisplayRole, BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), d.balance));
        table->setItem(i, COLUMN_BALANCE, balanceItem);

        // inputs
        auto *inputsItem = new NumberItem;
        inputsItem->amount = d.inputs;
        inputsItem->setData(Qt::DisplayRole, d.inputs);
        table->setItem(i, COLUMN_INPUTS, inputsItem);

        // copy item
        auto *copyItem = new QTableWidgetItem;
        copyItem->setToolTip(tr("Copy address"));
        auto *copyWidget = new QWidget();
        copyWidget->setContentsMargins(QMargins());
        auto *copyLayout = new QVBoxLayout;
        copyLayout->setContentsMargins(QMargins());
        copyLayout->setSpacing(0);
        copyWidget->setLayout(copyLayout);

        auto *copyButton = new BlocknetLabelBtn;
        copyButton->setText(tr("Copy"));
        copyButton->setID(d.address);
        copyLayout->addWidget(copyButton, 0, Qt::AlignLeft);
        copyLayout->addSpacing(BGU::spi(6));
        connect(copyButton, &BlocknetLabelBtn::clicked, this, &BlocknetAccounts::onCopyAddress);

        table->setCellWidget(i, COLUMN_COPY, copyWidget);
        table->setItem(i, COLUMN_COPY, copyItem);

        // edit item
        auto *editItem = new QTableWidgetItem;
        editItem->setToolTip(tr("Edit address alias"));
        auto *editWidget = new QWidget();
        editWidget->setContentsMargins(QMargins());
        auto *editLayout = new QVBoxLayout;
        editLayout->setContentsMargins(QMargins());
        editLayout->setSpacing(0);
        editWidget->setLayout(editLayout);

        auto *editButton = new BlocknetLabelBtn;
        editButton->setText(tr("Edit"));
        editButton->setID(d.address);
        editLayout->addWidget(editButton, 0, Qt::AlignLeft);
        editLayout->addSpacing(BGU::spi(6));
        connect(editButton, &BlocknetLabelBtn::clicked, this, &BlocknetAccounts::onEditAddress);

        table->setCellWidget(i, COLUMN_EDIT, editWidget);
        table->setItem(i, COLUMN_EDIT, editItem);
    }

    table->setSortingEnabled(true);
    QSettings settings;
    if (settings.contains("blocknetAccountsSortColumn") && settings.contains("blocknetAccountsSortOrder")) {
        const auto col = settings.value("blocknetAccountsSortColumn").toInt();
        const auto order = static_cast<Qt::SortOrder>(settings.value("blocknetAccountsSortOrder").toInt());
        table->horizontalHeader()->setSortIndicator(col, order);
    } else {
        table->horizontalHeader()->setSortIndicator(COLUMN_BALANCE, Qt::DescendingOrder);
        settings.setValue("blocknetAccountsSortColumn", COLUMN_BALANCE);
        settings.setValue("blocknetAccountsSortOrder", static_cast<int>(Qt::DescendingOrder));
    }
    watch();
}

void BlocknetAccounts::onCopyAddress() {
    auto *btn = qobject_cast<BlocknetLabelBtn*>(sender());
    auto address = btn->getID();
    GUIUtil::setClipboard(address);
}

void BlocknetAccounts::onAddAddress() {
    BlocknetAddressAddDialog dlg(walletModel->getAddressTableModel(), walletModel, Qt::WindowSystemMenuHint | Qt::WindowTitleHint);
    dlg.setStyleSheet(GUIUtil::loadStyleSheet());
    connect(&dlg, &BlocknetAddressAddDialog::rescan, [this](std::string walletName) {
        QTimer::singleShot(1000, [this,walletName]() {
            Q_EMIT rescan(walletName);
        });
    });
    dlg.exec();
}

void BlocknetAccounts::onEditAddress() {
    auto *btn = qobject_cast<BlocknetLabelBtn*>(sender());
    BlocknetAddressBook::Address data;
    data.address = btn->getID();
    // Remove address from data model
    auto rows = walletModel->getAddressTableModel()->rowCount(QModelIndex());
    for (int row = rows - 1; row >= 0; --row) {
        auto index = walletModel->getAddressTableModel()->index(row, 0, QModelIndex());
        auto *rec = static_cast<AddressTableEntry*>(index.internalPointer());
        if (rec && data.address == rec->address) {
            data.alias = rec->label;
            data.type = rec->type;
            break;
        }
    }
    BlocknetAddressEditDialog dlg(walletModel->getAddressTableModel(), walletModel, Qt::WindowSystemMenuHint | Qt::WindowTitleHint);
    dlg.setData(data.address, data.alias, data.type, QString());
    dlg.exec();
}

void BlocknetAccounts::onDoubleClick(int row, int col) {
    if (row >= filteredData.size()) // check index
        return;
    auto *item = table->item(row, COLUMN_ADDRESS);
    auto addr = item->data(Qt::DisplayRole).toString();
    for (auto & account : filteredData) {
        if (account.address.toStdString() == addr.toStdString()) {
            BlocknetAddressEditDialog dlg(walletModel->getAddressTableModel(), walletModel, Qt::WindowSystemMenuHint | Qt::WindowTitleHint);
            dlg.setData(account.address, account.alias, account.type, QString());
            dlg.exec();
            break;
        }
    }
}
