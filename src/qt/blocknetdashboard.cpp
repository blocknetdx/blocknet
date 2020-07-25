// Copyright (c) 2018-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetdashboard.h>

#include <qt/blocknethdiv.h>
#include <qt/blockneticonbtn.h>
#include <qt/blocknetguiutil.h>
#include <qt/blocknetvars.h>

#include <qt/bitcoinunits.h>
#include <qt/transactionrecord.h>
#include <qt/transactiontablemodel.h>

#include <QAbstractItemView>
#include <QDateTime>
#include <QFont>
#include <QHeaderView>

BlocknetDashboard::BlocknetDashboard(QFrame *parent) : QFrame(parent), layout(new QVBoxLayout),
                                                       walletModel(nullptr),
                                                       displayUnit(0), walletBalance(0),
                                                       unconfirmedBalance(0), immatureBalance(0) {
//    this->setStyleSheet("border: 1px solid red");
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->setContentsMargins(BGU::spi(46), BGU::spi(10), BGU::spi(50), 0);
    this->setLayout(layout);

    titleLbl = new QLabel(tr("Dashboard"));
    titleLbl->setObjectName("h4");

    auto *balanceGrid = new QFrame;
    auto *balanceGridLayout = new QGridLayout;
    balanceGridLayout->setContentsMargins(QMargins());
    balanceGrid->setLayout(balanceGridLayout);

    auto *balanceBox = new QFrame;
    auto *balanceBoxLayout = new QVBoxLayout;
    balanceBoxLayout->setContentsMargins(QMargins());
    balanceBox->setLayout(balanceBoxLayout);
    balanceLbl = new QLabel(tr("Available Balance"));
    balanceLbl->setObjectName("balanceLbl");
    balanceValueLbl = new QLabel;
    balanceValueLbl->setObjectName("h1");

    auto *pendingBox = new QFrame;
    auto *pendingLayout = new QHBoxLayout;
    pendingLayout->setContentsMargins(QMargins());
    pendingBox->setLayout(pendingLayout);
    pendingLbl = new QLabel(tr("Pending:"));
    pendingLbl->setObjectName("pendingLbl");
    pendingValueLbl = new QLabel;
    pendingValueLbl->setObjectName("pendingValueLbl");
    pendingLayout->addWidget(pendingLbl);
    pendingLayout->addWidget(pendingValueLbl, 0, Qt::AlignLeft);
    pendingLayout->addStretch(1);

    auto *immatureBox = new QFrame;
    auto *immatureLayout = new QHBoxLayout;
    immatureLayout->setContentsMargins(QMargins());
    immatureBox->setLayout(immatureLayout);
    immatureLbl = new QLabel(tr("Immature:"));
    immatureLbl->setObjectName("immatureLbl");
    immatureValueLbl = new QLabel;
    immatureValueLbl->setObjectName("immatureValueLbl");
    immatureLayout->addWidget(immatureLbl);
    immatureLayout->addWidget(immatureValueLbl, 0, Qt::AlignLeft);
    immatureLayout->addStretch(1);

    auto *totalBox = new QFrame;
    auto *totalLayout = new QHBoxLayout;
    totalLayout->setContentsMargins(QMargins());
    totalBox->setLayout(totalLayout);
    totalLbl = new QLabel(tr("Total:"));
    totalLbl->setObjectName("totalLbl");
    totalValueLbl = new QLabel;
    totalValueLbl->setObjectName("totalValueLbl");
    totalLayout->addWidget(totalLbl);
    totalLayout->addWidget(totalValueLbl, 0, Qt::AlignLeft);
    totalLayout->addStretch(1);

    balanceBoxLayout->addWidget(balanceValueLbl);
    balanceBoxLayout->addWidget(pendingBox);
    balanceBoxLayout->addWidget(immatureBox);
    balanceBoxLayout->addWidget(totalBox);

    auto *quickSend = new BlocknetIconBtn(tr("Quick Send"), ":/redesign/QuickActions/QuickSendIcon.png");

    balanceGridLayout->addWidget(balanceBox, 0, 0, Qt::AlignLeft);
    balanceGridLayout->addWidget(quickSend, 0, 1, Qt::AlignRight | Qt::AlignVCenter);
    balanceGridLayout->setColumnStretch(1, 1);

    auto *hdiv = new BlocknetHDiv;

    auto *recentBox = new QFrame;
    auto *recentLayout = new QHBoxLayout;
    recentLayout->setContentsMargins(QMargins());
    recentLayout->setSpacing(0);
    recentBox->setLayout(recentLayout);
    recentTxsLbl = new QLabel(tr("Recent Transactions"));
    recentTxsLbl->setObjectName("recentTransactionsLbl");
    viewAll = new QPushButton;
    viewAll->setObjectName("linkBtn");
    viewAll->setText(tr("View All"));
    viewAll->setCursor(Qt::PointingHandCursor);
    recentLayout->addWidget(recentTxsLbl, 0, Qt::AlignBottom);
    recentLayout->addWidget(viewAll, 0, Qt::AlignRight | Qt::AlignBottom);

    recentTransactions = new QFrame;
    recentTransactions->setObjectName("content");
    recentTransactions->setContentsMargins(QMargins());
    recentTransactions->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *recentTransactionsGridLayout = new QVBoxLayout;
    recentTransactionsGridLayout->setContentsMargins(QMargins());
    recentTransactions->setLayout(recentTransactionsGridLayout);
    table = new QTableWidget;
    table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    table->setContentsMargins(QMargins());
    table->setColumnCount(COLUMN_PADDING4 + 1);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setFocusPolicy(Qt::NoFocus);
    table->setAlternatingRowColors(true);
    table->setColumnWidth(COLUMN_PADDING1, 1);
    table->setColumnWidth(COLUMN_PADDING2, 1);
    table->setColumnWidth(COLUMN_PADDING3, 1);
    table->setColumnWidth(COLUMN_PADDING4, 1);
    table->setColumnWidth(COLUMN_STATUS, BGU::spi(3));
    table->setColumnWidth(COLUMN_DATE, BGU::spi(60));
    table->setColumnWidth(COLUMN_TIME, BGU::spi(72));
    table->setShowGrid(false);
    table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setContextMenuPolicy(Qt::CustomContextMenu);
    table->setSortingEnabled(false);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->verticalHeader()->setDefaultSectionSize(BGU::spi(58));
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    table->horizontalHeader()->setSortIndicatorShown(false);
    table->horizontalHeader()->setSectionsClickable(false);
    table->horizontalHeader()->setVisible(false);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_PADDING1, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_PADDING2, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_PADDING3, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_PADDING4, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_STATUS, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_DATE, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_TIME, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_TYPE, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_AMOUNT, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_TOADDRESS, QHeaderView::Stretch);
    table->setItemDelegateForColumn(COLUMN_STATUS, new BlocknetDashboardCellItem);
    table->setItemDelegateForColumn(COLUMN_DATE, new BlocknetDashboardCellItem);
    recentTransactionsGridLayout->addWidget(table, 1);

    layout->addWidget(titleLbl, 0, Qt::AlignTop | Qt::AlignLeft);
    layout->addWidget(balanceLbl);
    layout->addWidget(balanceGrid);
    layout->addSpacing(BGU::spi(15));
    layout->addWidget(hdiv);
    layout->addSpacing(BGU::spi(15));
    layout->addWidget(recentBox);
    layout->addWidget(recentTransactions, 1);
    layout->addSpacing(BGU::spi(20));

    connect(quickSend, &BlocknetIconBtn::clicked, this, &BlocknetDashboard::onQuickSend);
    connect(viewAll, &QPushButton::clicked, this, &BlocknetDashboard::onViewAll);
}

void BlocknetDashboard::setWalletModel(WalletModel *w) {
    if (walletModel == w) {
        displayUnit = walletModel->getOptionsModel()->getDisplayUnit();
        balanceChanged(walletModel->wallet().getBalances());
        return;
    }

    walletEvents(false);

    walletModel = w;
    if (!walletModel || !walletModel->getOptionsModel())
        return;

    displayUnit = walletModel->getOptionsModel()->getDisplayUnit();
    balanceChanged(walletModel->wallet().getBalances());

    auto *tableModel = walletModel->getTransactionTableModel();
    filter = new BlocknetDashboardFilterProxy(walletModel->getOptionsModel(), this);
    filter->setSourceModel(tableModel);
    filter->setLimit(30);
    filter->setDynamicSortFilter(true);
    filter->setSortRole(Qt::EditRole);
    filter->setFilterRole(Qt::EditRole);
    filter->sort(BlocknetDashboardFilterProxy::DashboardDate, Qt::DescendingOrder);

    initialize();

    // Watch for wallet changes
    walletEvents(true);

    connect(filter, &BlocknetDashboardFilterProxy::dataChanged, this, [this]() {
        refreshTableData();
    });
}

void BlocknetDashboard::initialize() {
    if (!walletModel)
        return;

    dataModel.clear();

    // Set up transaction list
    auto *tableModel = walletModel->getTransactionTableModel();
    for (int row = 0; row < filter->rowCount(QModelIndex()); ++row) {
        BlocknetDashboard::DashboardTx tx;
        updateData(row, tableModel, tx);
        dataModel << tx;
    }

    this->setData(dataModel);
}

void BlocknetDashboard::setData(const QVector<DashboardTx> & data) {
    walletEvents(false);
    table->clearContents();
    table->setRowCount(data.count());

    for (int i = 0; i < data.count(); ++i) {
        auto & d = data[i];
        addData(i, d);
    }

    walletEvents(true);
}

void BlocknetDashboard::refreshTableData() {
    walletEvents(false);
    // Add new rows
    const int filterRows = filter->rowCount(QModelIndex());
    const int tableRows = table->rowCount();
    if (filterRows > tableRows) {
        table->setRowCount(filterRows);
        for (int row = tableRows; row < filterRows; ++row) {
            BlocknetDashboard::DashboardTx d;
            addData(row, d);
            dataModel << d;
        }
    }
    // Update existing rows
    for (int row = tableRows - 1; row >= 0; --row) {
        if (filterRows < row) {
            table->removeRow(row);
            dataModel.removeAt(row);
            table->setRowCount(row);
            continue;
        }
        auto & d = dataModel[row];
        updateData(row, walletModel->getTransactionTableModel(), d);
        table->item(row, COLUMN_STATUS)->setData(Qt::EditRole, d.status);
        table->item(row, COLUMN_DATE)->setData(Qt::DisplayRole, d.date);
        table->item(row, COLUMN_DATE)->setData(Qt::EditRole, d.datetime);
        table->item(row, COLUMN_TIME)->setData(Qt::DisplayRole, d.time);
        table->item(row, COLUMN_TOADDRESS)->setData(Qt::DisplayRole, d.address);
        table->item(row, COLUMN_TYPE)->setData(Qt::DisplayRole, d.type);
        table->item(row, COLUMN_AMOUNT)->setData(Qt::DisplayRole, d.amount);
        table->item(row, COLUMN_AMOUNT)->setData(Qt::ForegroundRole, d.amountColor);
        // tooltip
        table->item(row, COLUMN_STATUS)->setData(Qt::ToolTipRole, d.tooltip);
        table->item(row, COLUMN_DATE)->setData(Qt::ToolTipRole, d.tooltip);
        table->item(row, COLUMN_TIME)->setData(Qt::ToolTipRole, d.tooltip);
        table->item(row, COLUMN_TOADDRESS)->setData(Qt::ToolTipRole, d.tooltip);
        table->item(row, COLUMN_TYPE)->setData(Qt::ToolTipRole, d.tooltip);
        table->item(row, COLUMN_AMOUNT)->setData(Qt::ToolTipRole, d.tooltip);
    }
    walletEvents(true);
}

void BlocknetDashboard::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    refreshTableData(); // turns on wallet events
}

void BlocknetDashboard::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    walletEvents(false);
}

void BlocknetDashboard::balanceChanged(const interfaces::WalletBalances & balances) {
    walletBalance = balances.balance;
    unconfirmedBalance = balances.unconfirmed_balance;
    immatureBalance = balances.immature_balance;
    updateBalance();
}

void BlocknetDashboard::displayUnitChanged(int unit) {
    displayUnit = unit;
    updateBalance();
}

void BlocknetDashboard::updateBalance() {
    balanceValueLbl->setText(BitcoinUnits::formatWithUnit(displayUnit, walletBalance, false, BitcoinUnits::separatorAlways));
    pendingValueLbl->setText(BitcoinUnits::formatWithUnit(displayUnit, unconfirmedBalance, false, BitcoinUnits::separatorAlways));
    immatureValueLbl->setText(BitcoinUnits::formatWithUnit(displayUnit, immatureBalance, false, BitcoinUnits::separatorAlways));
    totalValueLbl->setText(BitcoinUnits::formatWithUnit(displayUnit, walletBalance + unconfirmedBalance + immatureBalance, false, BitcoinUnits::separatorAlways));
}

void BlocknetDashboard::walletEvents(const bool on) {
    table->blockSignals(true);
    if (walletModel && on) {
        connect(walletModel->getOptionsModel(), &OptionsModel::displayUnitChanged, this, &BlocknetDashboard::displayUnitChanged);
        displayUnitChanged(walletModel->getOptionsModel()->getDisplayUnit());
    } else if (walletModel) {
        disconnect(walletModel->getOptionsModel(), &OptionsModel::displayUnitChanged, this, &BlocknetDashboard::displayUnitChanged);
    }
    table->blockSignals(false);
}

void BlocknetDashboard::updateData(int row, TransactionTableModel *tableModel, DashboardTx & d) {
    auto index = filter->index(row, 0);
    auto sourceIndex = filter->mapToSource(index);
    auto *rec = static_cast<TransactionRecord*>(tableModel->index(sourceIndex.row(), 0).internalPointer());
    if (!rec)
        return;

    auto datetime = rec->time;
    auto date = tableModel->formatTxDate(rec);
    auto time = QDateTime::fromTime_t(static_cast<uint>(rec->time)).toString("h:mmap");
    auto addr = tableModel->formatTxToAddress(rec, false);
    if (addr.isEmpty())
        addr = tr("n/a");
    auto type = tableModel->formatTxType(rec);
    auto amt = static_cast<CAmount>(rec->credit + rec->debit);
    auto amount = BitcoinUnits::floorWithUnit(displayUnit, amt, 6, true, BitcoinUnits::separatorNever);
    if (!rec->status.countsForBalance)
        amount = QString("[%1]").arg(amount);
    QColor amountColor("white");
    if ((rec->credit + rec->debit) < 0)
        amountColor = QColor(0xFB, 0x7F, 0x70);
    else if ((rec->credit + rec->debit) > 0)
        amountColor = QColor(0x4B, 0xF5, 0xC6);
    auto tooltip = tableModel->formatTooltip(rec);

    d.outpoint = COutPoint(uint256S(rec->getTxHash().toStdString()), rec->getOutputIndex());
    d.status = static_cast<int>(rec->status.status);
    d.datetime = datetime;
    d.date = date;
    d.time = time;
    d.address = addr;
    d.type = type;
    d.amount = amount;
    d.amountColor = amountColor;
    d.tooltip = tooltip;
}

void BlocknetDashboard::addData(int row, const DashboardTx & d) {
    QColor white("white");

    // color indicator
    auto *colorItem = new QTableWidgetItem;
    colorItem->setData(Qt::EditRole, d.status);
    table->setItem(row, COLUMN_STATUS, colorItem);

    // date
    auto *dateItem = new QTableWidgetItem;
    dateItem->setData(Qt::DisplayRole, d.date);
    dateItem->setData(Qt::EditRole, d.datetime);
    table->setItem(row, COLUMN_DATE, dateItem);

    // time
    auto *timeItem = new QTableWidgetItem;
    timeItem->setData(Qt::DisplayRole, d.time);
    timeItem->setData(Qt::ForegroundRole, white);
    table->setItem(row, COLUMN_TIME, timeItem);

    // address
    auto *addressItem = new QTableWidgetItem;
    addressItem->setData(Qt::DisplayRole, d.address);
    addressItem->setData(Qt::ForegroundRole, white);
    table->setItem(row, COLUMN_TOADDRESS, addressItem);

    // type
    auto *typeItem = new QTableWidgetItem;
    typeItem->setData(Qt::DisplayRole, d.type);
    typeItem->setData(Qt::ForegroundRole, white);
    table->setItem(row, COLUMN_TYPE, typeItem);

    // amount
    auto *amountItem = new QTableWidgetItem;
    amountItem->setData(Qt::DisplayRole, d.amount);
    amountItem->setData(Qt::ForegroundRole, d.amountColor);
    table->setItem(row, COLUMN_AMOUNT, amountItem);
}

BlocknetDashboardFilterProxy::BlocknetDashboardFilterProxy(OptionsModel *o, QObject *parent) : QSortFilterProxyModel(parent),
                                                                                               optionsModel(o),
                                                                                               limitRows(-1) { }


void BlocknetDashboardFilterProxy::setLimit(int limit) {
    this->limitRows = limit;
}

bool BlocknetDashboardFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);

    auto involvesWatchAddress = index.data(TransactionTableModel::WatchonlyRole).toBool();
    if (involvesWatchAddress)
        return false;

    auto status = index.data(TransactionTableModel::StatusRole).toInt();
    if (status == TransactionStatus::Conflicted)
        return false;

    return true;
}

bool BlocknetDashboardFilterProxy::lessThan(const QModelIndex &left, const QModelIndex &right) const {
    if (left.column() == BlocknetDashboardFilterProxy::DashboardDate) {
        QVariant leftData = sourceModel()->index(left.row(), TransactionTableModel::Date).data(Qt::EditRole);
        QVariant rightData = sourceModel()->index(right.row(), TransactionTableModel::Date).data(Qt::EditRole);
        return leftData.toLongLong() < rightData.toLongLong();
    }
    return QSortFilterProxyModel::lessThan(left, right);
}

int BlocknetDashboardFilterProxy::columnCount(const QModelIndex &) const {
    return BlocknetDashboardFilterProxy::DashboardAmount + 1;
}

int BlocknetDashboardFilterProxy::rowCount(const QModelIndex& parent) const {
    if (limitRows != -1)
        return std::min(QSortFilterProxyModel::rowCount(parent), limitRows);
    else
        return QSortFilterProxyModel::rowCount(parent);
}

QVariant BlocknetDashboardFilterProxy::data(const QModelIndex &index, int role) const {
    if (!index.isValid())
        return QVariant();

    auto *model = dynamic_cast<TransactionTableModel*>(sourceModel());
    QModelIndex sourceIndex = mapToSource(index);
    auto *rec = static_cast<TransactionRecord*>(model->index(sourceIndex.row(), 0).internalPointer());
    if (!rec)
        return QVariant();

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
            case DashboardStatus:
                return static_cast<int>(rec->status.status);
            case DashboardDate:
                return model->formatTxDate(rec);
            case DashboardTime:
                return QDateTime::fromTime_t(static_cast<uint>(rec->time)).toString("h:mmap");
            case DashboardToAddress: {
                auto addr = model->formatTxToAddress(rec, false);
                if (addr.isEmpty())
                    addr = tr("n/a");
                return addr;
            }
            case DashboardType:
                return model->formatTxType(rec);
            case DashboardAmount: {
                auto amt = static_cast<CAmount>(rec->credit + rec->debit);
                auto str = BitcoinUnits::floorWithUnit(optionsModel->getDisplayUnit(), amt, 2, true, BitcoinUnits::separatorNever);
                if (!rec->status.countsForBalance)
                    str = QString("[%1]").arg(str);
                return str;
            }
            default:
                return "";
        }
        break;
    case Qt::EditRole: // Edit role is used for sorting, so return the unformatted values
    case Qt::UserRole:
        switch (index.column()) {
            case DashboardStatus:
                return QString::fromStdString(rec->status.sortKey);
            case DashboardDate: {
                return rec->time;
            }
            case DashboardTime:
                return rec->time;
            case DashboardType:
                return model->formatTxType(rec);
            case DashboardToAddress:
                return model->formatTxToAddress(rec, true);
            case DashboardAmount:
                return static_cast<qint64>(rec->credit + rec->debit);
            default:
                return "";
            }
        break;
    case Qt::DecorationRole:
        switch (index.column()) {
            case DashboardStatus:
                return model->txStatusDecoration(rec);
            default:
                return QVariant();
        }
        break;
    case Qt::ToolTipRole:
        return model->formatTooltip(rec);
    case Qt::TextAlignmentRole:
        switch (index.column()) {
            case DashboardStatus:
                return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
            case DashboardDate:
            case DashboardTime:
                return static_cast<int>(Qt::AlignCenter | Qt::AlignVCenter);
            case DashboardType:
            case DashboardToAddress:
            case DashboardAmount:
            default:
                return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
        }
        break;
    case Qt::ForegroundRole:
        if (index.column() == DashboardAmount) {
            if ((rec->credit + rec->debit) < 0)
                return QColor(0xFB, 0x7F, 0x70);
            else if ((rec->credit + rec->debit) > 0)
                return QColor(0x4B, 0xF5, 0xC6);
        }
        return QColor("white");
    }
    return QSortFilterProxyModel::data(index, role);
}

BlocknetDashboardCellItem::BlocknetDashboardCellItem(QObject *parent) : QStyledItemDelegate(parent) { }

void BlocknetDashboardCellItem::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    painter->save();

    switch (index.column()) {
        case BlocknetDashboard::COLUMN_STATUS: {
            QColor color;
            auto status = static_cast<TransactionStatus::Status>(index.data(Qt::EditRole).toInt());
            switch (status) {
                case TransactionStatus::Status::Confirmed:
                case TransactionStatus::Status::Confirming:
                    color.setRgb(0x4B, 0xF5, 0xC6);
                    break;
                case TransactionStatus::Status::OpenUntilDate:
                case TransactionStatus::Status::OpenUntilBlock:
                case TransactionStatus::Status::Unconfirmed:
                case TransactionStatus::Status::Immature:
                    color.setRgb(0xF8, 0xBF, 0x1C);
                    break;
                case TransactionStatus::Status::Conflicted:
                case TransactionStatus::Status::NotAccepted:
                default:
                    color.setRgb(0xFB, 0x7F, 0x70);
                    break;
            }
            // Draw the status indicator, leave some room on top and bottom
            int pad = BGU::spi(2);
            QRect r(option.rect.x(), option.rect.y()+pad/2, BGU::spi(3), option.rect.height()-pad);
            painter->fillRect(r, color);
            break;
        }
        case BlocknetDashboard::COLUMN_DATE: {
            auto date = QDateTime::fromTime_t(index.data(Qt::EditRole).toULongLong());
            auto month = date.toString("MMM").toUpper();
            auto dt = date.toString("dd");
            painter->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
            // Draw the month
            painter->save();
            painter->setPen(QColor("white"));
            painter->setFont(QFont("Roboto", 13, 25));
            painter->drawText(QRect(option.rect.x(), option.rect.y() + BGU::spi(5), option.rect.width(), option.rect.height()*0.4), month, Qt::AlignCenter | Qt::AlignVCenter);
            painter->restore();
            // Draw the date
            painter->save();
            painter->setFont(QFont("Roboto", 21, 25));
            painter->setPen(QColor("white"));
            painter->drawText(QRect(option.rect.x(), option.rect.y() + BGU::spi(20), option.rect.width(), option.rect.height()*0.6), dt, Qt::AlignCenter | Qt::AlignVCenter);
            painter->restore();
            break;
        }
    }

    painter->restore();
}

QSize BlocknetDashboardCellItem::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const {
    switch (index.column()) {
        case BlocknetDashboardFilterProxy::DashboardStatus:
            return {BGU::spi(3), option.rect.height()};
        case BlocknetDashboardFilterProxy::DashboardDate:
            return {BGU::spi(60), option.rect.height()};
    }
    return QStyledItemDelegate::sizeHint(option, index);
}
