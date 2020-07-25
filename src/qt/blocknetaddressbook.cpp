// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetaddressbook.h>

#include <qt/blocknetaddressedit.h>
#include <qt/blocknetavatar.h>
#include <qt/blocknetguiutil.h>
#include <qt/blockneticonbtn.h>
#include <qt/blocknetlabelbtn.h>

#include <qt/addresstablemodel.h>
#include <qt/guiutil.h>

#include <QHeaderView>
#include <QSettings>
#include <QTimer>

BlocknetAddressBookDialog::BlocknetAddressBookDialog(WalletModel *model, Qt::WindowFlags f, int filter, QWidget *parent) : QDialog(parent, f) {
    this->setModal(true);
    if (filter == BlocknetAddressBook::FILTER_DEFAULT)
        this->setMinimumSize(BGU::spi(520), BGU::spi(650));
    else this->setMinimumSize(BGU::spi(700), BGU::spi(650));
    this->setStyleSheet(GUIUtil::loadStyleSheet());
    this->setContentsMargins(QMargins());
    this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    this->setWindowTitle(tr("Address Book"));

    auto *dialogLayout = new QVBoxLayout;
    dialogLayout->setContentsMargins(QMargins());
    this->setLayout(dialogLayout);

    auto *doneBtn = new BlocknetFormBtn;
    doneBtn->setText(tr("Done"));

    bool slimMode = filter == BlocknetAddressBook::FILTER_DEFAULT; // only slim if default filter
    form = new BlocknetAddressBook(slimMode, filter, this);
    form->setWalletModel(model);

    dialogLayout->addWidget(form, 1);
    dialogLayout->addWidget(doneBtn, 0, Qt::AlignCenter);
    dialogLayout->addSpacing(BGU::spi(20));

    connect(form, &BlocknetAddressBook::send, this, [this](const QString &address) {
        Q_EMIT send(address);
        if (ssMode)
            accept();
    });
    connect(doneBtn, &BlocknetFormBtn::clicked, this, [this]() {
        accept();
    });
}

void BlocknetAddressBookDialog::resizeEvent(QResizeEvent *evt) {
    QDialog::resizeEvent(evt);
}

BlocknetAddressBook::BlocknetAddressBook(bool slimMode, int filter, QWidget *parent) : QFrame(parent),
                                                                                       slimMode(slimMode),
                                                                                       layout(new QVBoxLayout) {
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    if (slimMode || filter != FILTER_DEFAULT)
        layout->setContentsMargins(0, BGU::spi(10), 0, 0);
    else layout->setContentsMargins(BGU::spi(46), BGU::spi(10), BGU::spi(50), 0);
    this->setLayout(layout);

    titleLbl = new QLabel(tr("Address Book"));
    titleLbl->setObjectName("h4");
    titleLbl->setFixedHeight(BGU::spi(26));
    if (slimMode || filter != FILTER_DEFAULT)
        titleLbl->hide();

    auto *topBox = new QFrame;
    topBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
    auto *topBoxLayout = new QHBoxLayout;
    topBoxLayout->setContentsMargins(QMargins());
    topBoxLayout->setSizeConstraint(QLayout::SetMaximumSize);
    topBox->setLayout(topBoxLayout);

    auto *addAddressBtn = new BlocknetIconBtn(":/redesign/QuickActions/AddressButtonIcon.png");

    addButtonLbl = new QLabel(tr("Create New Address"));
    addButtonLbl->setObjectName("h4");

    filterLbl = new QLabel(tr("Filter by:"));
    filterLbl->setObjectName("title");

    addressDropdown = new BlocknetDropdown;
    addressDropdown->addItem(tr("All Addresses"), FILTER_ALL);
    addressDropdown->addItem(tr("Contacts"),      FILTER_SENDING);
    addressDropdown->addItem(tr("My Addresses"),  FILTER_RECEIVING);

    table = new QTableWidget;
    table->setContentsMargins(QMargins());
    table->setColumnCount(COLUMN_DELETE + 1);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setFocusPolicy(Qt::NoFocus);
    table->setAlternatingRowColors(true);
    table->setColumnWidth(COLUMN_PADDING1, BGU::spi(1));
    table->setColumnWidth(COLUMN_PADDING2, BGU::spi(1));
    table->setColumnWidth(COLUMN_PADDING3, BGU::spi(1));
    table->setColumnWidth(COLUMN_ACTION, BGU::spi(40));
    table->setColumnWidth(COLUMN_AVATAR, BGU::spi(50));
    table->setColumnWidth(COLUMN_COPY, BGU::spi(65));
    table->setColumnWidth(COLUMN_EDIT, BGU::spi(65));
    table->setColumnWidth(COLUMN_DELETE, BGU::spi(65));
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
    table->horizontalHeader()->setSectionResizeMode(COLUMN_ACTION, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_AVATAR, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_ALIAS, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_ADDRESS, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_COPY, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_EDIT, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_DELETE, QHeaderView::Fixed);
    table->setHorizontalHeaderLabels({ "", "", "", tr("Alias"), "", tr("Address"), "", "", "", "" });

    // If in slim mode, hide all columns except add, alias, and address
    if (slimMode) {
        table->setColumnHidden(COLUMN_COPY, true);
        table->setColumnHidden(COLUMN_EDIT, true);
        table->setColumnHidden(COLUMN_DELETE, true);
    }

    if (!slimMode) {
        if (filter != FILTER_DEFAULT)
            topBoxLayout->addSpacing(BGU::spi(30));
        topBoxLayout->addWidget(addAddressBtn, 0, Qt::AlignLeft);
        topBoxLayout->addWidget(addButtonLbl, 0, Qt::AlignLeft | Qt::AlignVCenter);
        topBoxLayout->addStretch(1);
    }
    topBoxLayout->addWidget(filterLbl, 0, Qt::AlignRight);
    topBoxLayout->addWidget(addressDropdown, 0, Qt::AlignRight);
    if (filter != FILTER_DEFAULT)
        topBoxLayout->addSpacing(BGU::spi(30));

    layout->addWidget(titleLbl);
    layout->addSpacing(BGU::spi(10));
    if (!slimMode)
        layout->addWidget(topBox);
    else layout->addWidget(topBox, 0, Qt::AlignCenter);
    layout->addSpacing(BGU::spi(15));
    layout->addWidget(table, 1);
    layout->addSpacing(BGU::spi(20));

    // Set the filter dropdown state
    if (filter == FILTER_DEFAULT) {
        int ddIndex = 0;
        QSettings settings;
        if (!slimMode)
            ddIndex = settings.value("blocknetAddressBookFilter").toInt();
        else ddIndex = settings.value("blocknetAddressBookFilterSlim").toInt();
        addressDropdown->setCurrentIndex(ddIndex);
    }
    else {
        addressDropdown->setCurrentIndex(ddIndexForType(filter));
    }
    filteredOption = addressDropdown->currentData().toInt();

    connect(addAddressBtn, &BlocknetIconBtn::clicked, this, &BlocknetAddressBook::onAddAddress);
    connect(addressDropdown, &BlocknetDropdown::valueChanged, this, &BlocknetAddressBook::onFilter);
    connect(table, &QTableWidget::cellDoubleClicked, this, &BlocknetAddressBook::onDoubleClick);
    connect(table->horizontalHeader(), &QHeaderView::sortIndicatorChanged, this, [this](int column, Qt::SortOrder order) {
        QSettings settings;
        if (column <= COLUMN_PADDING1 || column == COLUMN_PADDING2 || column == COLUMN_PADDING3) {
            table->horizontalHeader()->setSortIndicator(settings.value("blocknetAddressBookSortColumn").toInt(),
                    static_cast<Qt::SortOrder>(settings.value("blocknetAddressBookSortOrder").toInt()));
            return;
        }
        settings.setValue("blocknetAddressBookSortOrder", static_cast<int>(order));
        settings.setValue("blocknetAddressBookSortColumn", column);
    });
}

void BlocknetAddressBook::setWalletModel(WalletModel *w) {
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

void BlocknetAddressBook::initialize() {
    if (!walletModel)
        return;

    dataModel.clear();

    AddressTableModel *addressTableModel = walletModel->getAddressTableModel();
    int rowCount = addressTableModel->rowCount(QModelIndex());

    for (int row=0; row<rowCount; row++) {
        auto index = addressTableModel->index(row, 0, QModelIndex());
        auto *rec = static_cast<AddressTableEntry*>(index.internalPointer());
        if (!rec)
            continue;
        QString alias = rec->label;
        QString address = rec->address;
        int type = rec->type;
        Address a = {
            alias,
            address,
            type
        };
        dataModel << a;
    }

    this->setData(filtered(dataModel, filteredOption));
}

void BlocknetAddressBook::unwatch() {
    table->setEnabled(false);
}

void BlocknetAddressBook::watch() {
    table->setEnabled(true);
}

QVector<BlocknetAddressBook::Address> BlocknetAddressBook::filtered(const QVector<Address> &data, int filter) {
    QVector<Address> r;
    for (auto &d : data) {
        switch (filter) {
            case AddressTableEntry::Sending:
                if (d.type == AddressTableEntry::Sending)
                    r.push_back(d);
                break;
            case AddressTableEntry::Receiving:
                if (d.type == AddressTableEntry::Receiving)
                    r.push_back(d);
                break;
            case FILTER_ALL: // must be 2 or greater
            default:
                r.push_back(d);
                break;
        }
    }
    return r;
}

void BlocknetAddressBook::setData(const QVector<Address> &data) {
    this->filteredData = data;

    unwatch();
    table->clearContents();
    table->setRowCount(this->filteredData.count());
    table->setSortingEnabled(false);

    for (int i = 0; i < this->filteredData.count(); ++i) {
        auto &d = this->filteredData[i];

        // action item
        auto *actionItem = new QTableWidgetItem;
        actionItem->setToolTip(!slimMode ? tr("Send funds to this address") : tr("Add this address"));
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
        connect(copyButton, &BlocknetLabelBtn::clicked, this, &BlocknetAddressBook::onCopyAddress);

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
        connect(editButton, &BlocknetLabelBtn::clicked, this, &BlocknetAddressBook::onEditAddress);

        table->setCellWidget(i, COLUMN_EDIT, editWidget);
        table->setItem(i, COLUMN_EDIT, editItem);

        // delete item
        auto *deleteItem = new QTableWidgetItem;
        deleteItem->setToolTip(tr("Delete this address"));
        auto *deleteWidget = new QWidget();
        deleteWidget->setContentsMargins(QMargins());
        auto *deleteLayout = new QVBoxLayout;
        deleteLayout->setContentsMargins(QMargins());
        deleteLayout->setSpacing(0);
        deleteWidget->setLayout(deleteLayout);

        // Can only delete sending addresses
        if (d.type == AddressTableEntry::Sending) {
            auto *deleteButton = new BlocknetLabelBtn;
            deleteButton->setText(tr("Delete"));
            deleteButton->setID(d.address);
            deleteLayout->addWidget(deleteButton, 0, Qt::AlignLeft);
            deleteLayout->addSpacing(BGU::spi(6));
            connect(deleteButton, &BlocknetLabelBtn::clicked, this, &BlocknetAddressBook::onDeleteAddress);
        }

        table->setCellWidget(i, COLUMN_DELETE, deleteWidget);
        table->setItem(i, COLUMN_DELETE, deleteItem);
    }

    table->setSortingEnabled(true);
    QSettings settings;
    if (settings.contains("blocknetAddressBookSortColumn") && settings.contains("blocknetAddressBookSortOrder")) {
        const auto col = settings.value("blocknetAddressBookSortColumn").toInt();
        const auto order = static_cast<Qt::SortOrder>(settings.value("blocknetAddressBookSortOrder").toInt());
        table->horizontalHeader()->setSortIndicator(col, order);
    } else {
        table->horizontalHeader()->setSortIndicator(COLUMN_ALIAS, Qt::AscendingOrder);
        settings.setValue("blocknetAddressBookSortColumn", COLUMN_ALIAS);
        settings.setValue("blocknetAddressBookSortOrder", static_cast<int>(Qt::AscendingOrder));
    }
    watch();
}

/**
 * @brief Filters the data model based on the current filter dropdown filter flag.
 */
void BlocknetAddressBook::onFilter() {
    filteredOption = addressDropdown->currentData().toInt();
    setData(filtered(dataModel, filteredOption));
    QSettings settings;
    if (!slimMode)
        settings.setValue("blocknetAddressBookFilter", addressDropdown->currentIndex());
    else settings.setValue("blocknetAddressBookFilterSlim", addressDropdown->currentIndex());
}

void BlocknetAddressBook::onCopyAddress() {
    auto *btn = qobject_cast<BlocknetLabelBtn*>(sender());
    auto address = btn->getID();
    GUIUtil::setClipboard(address);
}

void BlocknetAddressBook::onAddAddress() {
    BlocknetAddressAddDialog dlg(walletModel->getAddressTableModel(), walletModel, Qt::WindowSystemMenuHint | Qt::WindowTitleHint);
    dlg.setStyleSheet(GUIUtil::loadStyleSheet());
    connect(&dlg, &BlocknetAddressAddDialog::rescan, [this](std::string walletName) {
        QTimer::singleShot(1000, [this,walletName]() {
            Q_EMIT rescan(walletName);
        });
    });
    dlg.exec();
}

void BlocknetAddressBook::onEditAddress() {
    auto *btn = qobject_cast<BlocknetLabelBtn*>(sender());
    Address data;
    data.address = btn->getID();
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

void BlocknetAddressBook::onDoubleClick(int row, int col) {
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

void BlocknetAddressBook::onDeleteAddress() {
    auto *btn = qobject_cast<BlocknetLabelBtn*>(sender());
    auto address = btn->getID();
    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Delete Address"),
                                                               QString("%1\n\n%2").arg(tr("Are you sure you want to delete this address?"), address),
                                                               QMessageBox::Yes | QMessageBox::No,
                                                               QMessageBox::No);

    if (retval != QMessageBox::Yes)
        return;

    // Remove address from data model
    auto rows = walletModel->getAddressTableModel()->rowCount(QModelIndex());
    for (int row = rows - 1; row >= 0; --row) {
        auto index = walletModel->getAddressTableModel()->index(row, AddressTableModel::Address, QModelIndex());
        if (address == index.data(Qt::EditRole).toString()) {
            walletModel->getAddressTableModel()->removeRows(index.row(), 1, index.parent());
            break;
        }
    }
}

void BlocknetAddressBook::onAddressAction() {
    auto *btn = qobject_cast<BlocknetActionBtn*>(sender());
    auto address = btn->getID();
    Q_EMIT send(address);
}

int BlocknetAddressBook::ddIndexForType(int type) {
    switch (type) {
        case FILTER_SENDING:
            return 1;
        case FILTER_RECEIVING:
            return 2;
        case FILTER_ALL:
        default:
            return 0;
    }
}
