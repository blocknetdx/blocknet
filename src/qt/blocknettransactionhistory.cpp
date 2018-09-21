// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknettransactionhistory.h"
#include "blocknetformbtn.h"
#include "blocknetvars.h"
#include "blocknetformbtn.h"

#include "bitcoinunits.h"
#include "transactionrecord.h"
#include "transactiontablemodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "transactiondescdialog.h"
#include "csvmodelwriter.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QFont>
#include <QDateTime>
#include <QSettings>

#include <QKeyEvent>

BlocknetTransactionHistory::BlocknetTransactionHistory(WalletModel *w, QWidget *parent) : QFrame(parent), walletModel(w),
                                                                                          layout(new QVBoxLayout) {
//    this->setStyleSheet("border: 1px solid red");
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(layout);
    layout->setContentsMargins(45, 10, 45, 30);

    displayUnit = walletModel->getOptionsModel()->getDisplayUnit();
    auto displayUnitName = BitcoinUnits::name(displayUnit);

    auto *titleBox = new QFrame;
    titleBox->setContentsMargins(QMargins());
    auto *titleBoxLayout = new QHBoxLayout;
    titleBox->setLayout(titleBoxLayout);
    titleBoxLayout->setContentsMargins(QMargins());
    titleLbl = new QLabel(tr("Transaction History"));
    titleLbl->setObjectName("h4");
    auto *exportBtn = new BlocknetFormBtn;
    exportBtn->setText(tr("Export History"));
    titleBoxLayout->addWidget(titleLbl, 0, Qt::AlignTop | Qt::AlignLeft);
    titleBoxLayout->addStretch(1);
    titleBoxLayout->addWidget(exportBtn, 0, Qt::AlignRight);

    QLabel *subtitleLbl = new QLabel;
    subtitleLbl->setObjectName("h2");

    auto *searchBox = new QFrame;
    searchBox->setContentsMargins(QMargins());
    searchBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *searchBoxLayout = new QHBoxLayout;
    searchBoxLayout->setContentsMargins(QMargins());
    searchBox->setLayout(searchBoxLayout);

    dateCb = new BlocknetDropdown;
    dateCb->addItem(tr("All"),        All);
    dateCb->addItem(tr("Today"),      Today);
    dateCb->addItem(tr("This week"),  ThisWeek);
    dateCb->addItem(tr("This month"), ThisMonth);
    dateCb->addItem(tr("Last month"), LastMonth);
    dateCb->addItem(tr("This year"),  ThisYear);
    dateCb->addItem(tr("Range..."),   Range);

    typeCb = new BlocknetDropdown;
    typeCb->addItem(tr("All"),                 BlocknetTransactionHistoryFilterProxy::ALL_TYPES);
    typeCb->addItem(tr("Most Common"),         BlocknetTransactionHistoryFilterProxy::COMMON_TYPES);
    typeCb->addItem(tr("Received"),            BlocknetTransactionHistoryFilterProxy::TYPE(TransactionRecord::RecvWithAddress) | BlocknetTransactionHistoryFilterProxy::TYPE(TransactionRecord::RecvFromOther));
    typeCb->addItem(tr("Sent"),                BlocknetTransactionHistoryFilterProxy::TYPE(TransactionRecord::SendToAddress) | BlocknetTransactionHistoryFilterProxy::TYPE(TransactionRecord::SendToOther));
    typeCb->addItem(tr("To yourself"),         BlocknetTransactionHistoryFilterProxy::TYPE(TransactionRecord::SendToSelf));
    typeCb->addItem(tr("Mined"),               BlocknetTransactionHistoryFilterProxy::TYPE(TransactionRecord::Generated));
    typeCb->addItem(tr("Minted"),              BlocknetTransactionHistoryFilterProxy::TYPE(TransactionRecord::StakeMint));
    typeCb->addItem(tr("Service Node Reward"), BlocknetTransactionHistoryFilterProxy::TYPE(TransactionRecord::MNReward));
    typeCb->addItem(tr("Other"),               BlocknetTransactionHistoryFilterProxy::TYPE(TransactionRecord::Other));

    addressTi = new BlocknetLineEdit;
    addressTi->setParent(this);
    addressTi->setPlaceholderText(tr("Enter address or label to search"));

    amountTi = new BlocknetLineEdit;
    amountTi->setFixedWidth(120);
    amountTi->setParent(this);
    amountTi->setPlaceholderText(tr("Min amount"));

    searchBoxLayout->addWidget(dateCb);
    searchBoxLayout->addWidget(typeCb);
    searchBoxLayout->addWidget(addressTi);
    searchBoxLayout->addWidget(amountTi);

    dateRangeWidget = new QFrame();
    dateRangeWidget->setObjectName("dateRange");
    dateRangeWidget->setContentsMargins(1, 1, 1, 1);
    auto *dateRangeWidgetLayout = new QHBoxLayout(dateRangeWidget);
    dateRangeWidgetLayout->setContentsMargins(QMargins());
    dateRangeWidgetLayout->addWidget(new QLabel(tr("Date from")));
    dateRangeWidget->setVisible(false); // Hide by default

    dateFrom = new QDateTimeEdit;
    dateFrom->setFixedSize(120, 30);
    dateFrom->setDisplayFormat("MM/dd/yy");
    dateFrom->setCalendarPopup(true);
    dateFrom->setMinimumWidth(100);
    dateFrom->setDate(QDate::currentDate().addDays(-7));
    dateRangeWidgetLayout->addWidget(dateFrom);
    dateRangeWidgetLayout->addWidget(new QLabel(tr("to")));

    dateTo = new QDateTimeEdit;
    dateTo->setFixedSize(120, 30);
    dateTo->setDisplayFormat("MM/dd/yy");
    dateTo->setCalendarPopup(true);
    dateTo->setMinimumWidth(100);
    dateTo->setDate(QDate::currentDate());
    dateRangeWidgetLayout->addWidget(dateTo);
    dateRangeWidgetLayout->addStretch();

    transactionsTbl = new BlocknetTransactionHistoryTable(this);
    transactionsTbl->setObjectName("transactionHistory");
    transactionsTbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    transactionsTbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
    transactionsTbl->setSelectionBehavior(QAbstractItemView::SelectRows);
    transactionsTbl->setSelectionMode(QAbstractItemView::ExtendedSelection);
    transactionsTbl->setAlternatingRowColors(true);
    transactionsTbl->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    transactionsTbl->setShowGrid(false);
    transactionsTbl->setFocusPolicy(Qt::NoFocus);
    transactionsTbl->setContextMenuPolicy(Qt::CustomContextMenu);
    transactionsTbl->setSortingEnabled(true);
    transactionsTbl->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    transactionsTbl->verticalHeader()->setDefaultSectionSize(60);
    transactionsTbl->verticalHeader()->setVisible(false);
    transactionsTbl->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    transactionsTbl->horizontalHeader()->setSortIndicatorShown(true);
    transactionsTbl->horizontalHeader()->setSectionsClickable(true);

    layout->addWidget(titleBox);
    layout->addSpacing(30);
    layout->addWidget(searchBox);
    layout->addWidget(dateRangeWidget);
    layout->addWidget(transactionsTbl);

    // Actions
    QAction *copyAddressAction = new QAction(tr("Copy address"), this);
    QAction *copyLabelAction = new QAction(tr("Copy label"), this);
    QAction *copyAmountAction = new QAction(tr("Copy amount"), this);
    QAction *copyTxIDAction = new QAction(tr("Copy transaction ID"), this);
    QAction *showDetailsAction = new QAction(tr("Show transaction details"), this);

    contextMenu = new QMenu;
    contextMenu->addAction(copyAddressAction);
    contextMenu->addAction(copyLabelAction);
    contextMenu->addAction(copyAmountAction);
    contextMenu->addAction(copyTxIDAction);
    contextMenu->addAction(showDetailsAction);

    displayUnit = walletModel->getOptionsModel()->getDisplayUnit();

    // Set transaction data and adjust section sizes
    transactionsTbl->setWalletModel(walletModel);
    transactionsTbl->horizontalHeader()->setSectionResizeMode(BlocknetTransactionHistoryFilterProxy::HistoryStatus, QHeaderView::Fixed);
    transactionsTbl->horizontalHeader()->setSectionResizeMode(BlocknetTransactionHistoryFilterProxy::HistoryDate, QHeaderView::Fixed);
    transactionsTbl->horizontalHeader()->setSectionResizeMode(BlocknetTransactionHistoryFilterProxy::HistoryTime, QHeaderView::Fixed);
    transactionsTbl->horizontalHeader()->setSectionResizeMode(BlocknetTransactionHistoryFilterProxy::HistoryToAddress, QHeaderView::Stretch);
    transactionsTbl->horizontalHeader()->setSectionResizeMode(BlocknetTransactionHistoryFilterProxy::HistoryType, QHeaderView::ResizeToContents);
    transactionsTbl->horizontalHeader()->setSectionResizeMode(BlocknetTransactionHistoryFilterProxy::HistoryAmount, QHeaderView::ResizeToContents);
    transactionsTbl->setColumnWidth(BlocknetTransactionHistoryFilterProxy::HistoryStatus, 3);
    transactionsTbl->setColumnWidth(BlocknetTransactionHistoryFilterProxy::HistoryDate, 60);
    transactionsTbl->setColumnWidth(BlocknetTransactionHistoryFilterProxy::HistoryTime, 72);

    // Restore from last visit
    QSettings settings;
    typeCb->setCurrentIndex(settings.value("transactionType").toInt());
    dateCb->setCurrentIndex(settings.value("transactionDate").toInt());
    dateChanged(settings.value("transactionDate").toInt());
    typeChanged(settings.value("transactionType").toInt());

    connect(copyAddressAction, SIGNAL(triggered()), this, SLOT(copyAddress()));
    connect(copyLabelAction, SIGNAL(triggered()), this, SLOT(copyLabel()));
    connect(copyAmountAction, SIGNAL(triggered()), this, SLOT(copyAmount()));
    connect(copyTxIDAction, SIGNAL(triggered()), this, SLOT(copyTxID()));
    connect(showDetailsAction, SIGNAL(triggered()), this, SLOT(showDetails(const QModelIndex &)));
    connect(dateFrom, SIGNAL(dateChanged(QDate)), this, SLOT(dateRangeChanged()));
    connect(dateTo, SIGNAL(dateChanged(QDate)), this, SLOT(dateRangeChanged()));
    connect(exportBtn, SIGNAL(clicked()), this, SLOT(exportClicked()));
}

void BlocknetTransactionHistory::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    connect(walletModel->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(onDisplayUnit(int)));
    connect(addressTi, SIGNAL(textChanged(QString)), this, SLOT(addressChanged(QString)));
    connect(amountTi, SIGNAL(textChanged(QString)), this, SLOT(amountChanged(QString)));
    connect(dateCb, SIGNAL(activated(int)), this, SLOT(dateChanged(int)));
    connect(typeCb, SIGNAL(activated(int)), this, SLOT(typeChanged(int)));
    connect(transactionsTbl, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(showDetails(const QModelIndex &)));
    connect(transactionsTbl, SIGNAL(clicked(QModelIndex)), this, SLOT(computeSum()));
    connect(transactionsTbl, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextualMenu(QPoint)));
}

void BlocknetTransactionHistory::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    disconnect(walletModel->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(onDisplayUnit(int)));
    disconnect(addressTi, SIGNAL(textChanged(QString)), this, SLOT(addressChanged(QString)));
    disconnect(amountTi, SIGNAL(textChanged(QString)), this, SLOT(amountChanged(QString)));
    disconnect(dateCb, SIGNAL(activated(int)), this, SLOT(dateChanged(int)));
    disconnect(typeCb, SIGNAL(activated(int)), this, SLOT(typeChanged(int)));
    disconnect(transactionsTbl, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(showDetails(const QModelIndex &)));
    disconnect(transactionsTbl, SIGNAL(clicked(QModelIndex)), this, SLOT(computeSum()));
    disconnect(transactionsTbl, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextualMenu(QPoint)));
}

void BlocknetTransactionHistory::onDisplayUnit(int unit) {
    displayUnit = unit;
    // TODO Reload table with new unit?
}

void BlocknetTransactionHistory::addressChanged(const QString &prefix) {
    transactionsTbl->setAddressPrefix(prefix);
}

void BlocknetTransactionHistory::amountChanged(const QString &amount) {
    CAmount amount_parsed = 0;
    QString newAmount = amount;
    newAmount.replace(QString(","), QString("."));

    if (BitcoinUnits::parse(displayUnit, newAmount, &amount_parsed)) {
        transactionsTbl->setMinAmount(amount_parsed);
    } else {
        transactionsTbl->setMinAmount(0);
    }
}

void BlocknetTransactionHistory::dateChanged(int idx) {
    QDate current = QDate::currentDate();
    dateRangeWidget->setVisible(false);
    switch (dateCb->itemData(idx).toInt()) {
        case All:
            transactionsTbl->setDateRange(
                    BlocknetTransactionHistoryFilterProxy::MIN_DATE,
                    BlocknetTransactionHistoryFilterProxy::MAX_DATE);
            break;
        case Today:
            transactionsTbl->setDateRange(
                    QDateTime(current),
                    BlocknetTransactionHistoryFilterProxy::MAX_DATE);
            break;
        case ThisWeek: {
            // Find last Monday
            QDate startOfWeek = current.addDays(-(current.dayOfWeek() - 1));
            transactionsTbl->setDateRange(
                    QDateTime(startOfWeek),
                    BlocknetTransactionHistoryFilterProxy::MAX_DATE);

        } break;
        case ThisMonth:
            transactionsTbl->setDateRange(
                    QDateTime(QDate(current.year(), current.month(), 1)),
                    BlocknetTransactionHistoryFilterProxy::MAX_DATE);
            break;
        case LastMonth:
            transactionsTbl->setDateRange(
                    QDateTime(QDate(current.year(), current.month() - 1, 1)),
                    QDateTime(QDate(current.year(), current.month(), 1)));
            break;
        case ThisYear:
            transactionsTbl->setDateRange(
                    QDateTime(QDate(current.year(), 1, 1)),
                    BlocknetTransactionHistoryFilterProxy::MAX_DATE);
            break;
        case Range:
            dateRangeWidget->setVisible(true);
            dateRangeChanged();
            break;
    }
    if (dateCb->itemData(idx).toInt() != Range) {
        QSettings settings;
        settings.setValue("transactionDate", idx);
    }
}

void BlocknetTransactionHistory::typeChanged(int idx) {
    transactionsTbl->setTypeFilter(static_cast<quint32>(typeCb->itemData(idx).toInt()));
    QSettings settings;
    settings.setValue("transactionType", idx);
}

void BlocknetTransactionHistory::dateRangeChanged() {
    transactionsTbl->setDateRange(QDateTime(dateFrom->date()),
                                  QDateTime(dateTo->date()).addDays(1));
}

void BlocknetTransactionHistory::contextualMenu(const QPoint& point) {
    auto index = transactionsTbl->indexAt(point);
    if (index.isValid())
        contextMenu->exec(QCursor::pos());
}

void BlocknetTransactionHistory::copyAddress() {
    GUIUtil::copyEntryData(transactionsTbl, 0, TransactionTableModel::AddressRole);
}

void BlocknetTransactionHistory::copyLabel() {
    GUIUtil::copyEntryData(transactionsTbl, 0, TransactionTableModel::LabelRole);
}

void BlocknetTransactionHistory::copyAmount() {
    GUIUtil::copyEntryData(transactionsTbl, 0, TransactionTableModel::FormattedAmountRole);
}

void BlocknetTransactionHistory::copyTxID() {
    GUIUtil::copyEntryData(transactionsTbl, 0, TransactionTableModel::TxIDRole);
}

void BlocknetTransactionHistory::showDetails(const QModelIndex &) {
    auto selection = transactionsTbl->selectionModel()->selectedRows();
    if (!selection.isEmpty()) {
        TransactionDescDialog dlg(selection.at(0));
        dlg.setStyleSheet(GUIUtil::loadStyleSheetv1());
        dlg.exec();
    }
}

void BlocknetTransactionHistory::exportClicked() {
    // CSV is currently the only supported format
    QString filename = GUIUtil::getSaveFileName(this, tr("Export Transaction History"), QString(),
                                                tr("Comma separated file (*.csv)"), NULL);

    if (filename.isNull())
        return;

    CSVModelWriter writer(filename);
    auto *model = transactionsTbl->model();

    // name, column, role
    writer.setModel(model);
    writer.addColumn(tr("Confirmed"), 0, TransactionTableModel::ConfirmedRole);
//    if (model && model->haveWatchOnly())
//        writer.addColumn(tr("Watch-only"), TransactionTableModel::Watchonly);
    writer.addColumn(tr("Date"), 0, TransactionTableModel::DateRole);
    writer.addColumn(tr("Type"), TransactionTableModel::Type, Qt::EditRole);
    writer.addColumn(tr("Label"), 0, TransactionTableModel::LabelRole);
    writer.addColumn(tr("Address"), 0, TransactionTableModel::AddressRole);
    writer.addColumn(BitcoinUnits::getAmountColumnTitle(displayUnit), 0, TransactionTableModel::FormattedAmountRole);
    writer.addColumn(tr("ID"), 0, TransactionTableModel::TxIDRole);

    if (!writer.write())
        QMessageBox::warning(this->parentWidget(), tr("Export Failed"), tr("There was an error trying to save the transaction history to %1.").arg(filename), QMessageBox::Ok);
    else
        QMessageBox::warning(this->parentWidget(), tr("Export Successful"), tr("The transaction history was successfully saved to %1.").arg(filename), QMessageBox::Ok);
}

BlocknetTransactionHistoryTable::BlocknetTransactionHistoryTable(QWidget *parent) : QTableView(parent),
                                                                                    walletModel(nullptr) {
}

void BlocknetTransactionHistoryTable::setWalletModel(WalletModel *w) {
    if (walletModel == w)
        return;
    walletModel = w;

    if (walletModel == nullptr) {
        setModel(nullptr);
        return;
    }

    this->setItemDelegateForColumn(BlocknetTransactionHistoryFilterProxy::HistoryStatus, new BlocknetTransactionHistoryCellItem(this));
    this->setItemDelegateForColumn(BlocknetTransactionHistoryFilterProxy::HistoryDate, new BlocknetTransactionHistoryCellItem(this));

    // Set up transaction list
    auto *filter = new BlocknetTransactionHistoryFilterProxy(walletModel->getOptionsModel(), this);
    filter->setSourceModel(walletModel->getTransactionTableModel());
    filter->setDynamicSortFilter(true);
    filter->setSortRole(Qt::EditRole);
    filter->setFilterRole(Qt::EditRole);
    filter->setSortCaseSensitivity(Qt::CaseInsensitive);
    filter->setFilterCaseSensitivity(Qt::CaseInsensitive);
    filter->sort(BlocknetTransactionHistoryFilterProxy::HistoryDate, Qt::DescendingOrder);
    setModel(filter);
}

void BlocknetTransactionHistoryTable::leave() {
    this->blockSignals(true);
    model()->blockSignals(true);
}
void BlocknetTransactionHistoryTable::enter() {
    this->blockSignals(false);
    model()->blockSignals(false);
}

void BlocknetTransactionHistoryTable::setAddressPrefix(const QString &prefix) {
    auto *m = dynamic_cast<BlocknetTransactionHistoryFilterProxy*>(this->model());
    m->setAddressPrefix(prefix);
}

void BlocknetTransactionHistoryTable::setMinAmount(const CAmount &minimum) {
    auto *m = dynamic_cast<BlocknetTransactionHistoryFilterProxy*>(this->model());
    m->setMinAmount(minimum);
}

void BlocknetTransactionHistoryTable::setTypeFilter(quint32 types) {
    auto *m = dynamic_cast<BlocknetTransactionHistoryFilterProxy*>(this->model());
    m->setTypeFilter(types);
}

void BlocknetTransactionHistoryTable::setDateRange(const QDateTime &from, const QDateTime &to) {
    auto *m = dynamic_cast<BlocknetTransactionHistoryFilterProxy*>(this->model());
    m->setDateRange(from, to);
}

const QDateTime BlocknetTransactionHistoryFilterProxy::MIN_DATE = QDateTime::fromTime_t(0);
const QDateTime BlocknetTransactionHistoryFilterProxy::MAX_DATE = QDateTime::fromTime_t(0xFFFFFFFF);
BlocknetTransactionHistoryFilterProxy::BlocknetTransactionHistoryFilterProxy(OptionsModel *o, QObject *parent) : QSortFilterProxyModel(parent),
                                                                                                                 optionsModel(o),
                                                                                                                 limitRows(-1),
                                                                                                                 addrPrefix(QString()),
                                                                                                                 minAmount(0),
                                                                                                                 typeFilter(COMMON_TYPES),
                                                                                                                 dateFrom(MIN_DATE),
                                                                                                                 dateTo(MAX_DATE) { }

void BlocknetTransactionHistoryFilterProxy::setLimit(int limit) {
    this->limitRows = limit;
}

void BlocknetTransactionHistoryFilterProxy::setAddressPrefix(const QString &prefix) {
    this->addrPrefix = prefix;
    invalidateFilter();
}

void BlocknetTransactionHistoryFilterProxy::setMinAmount(const CAmount &minimum) {
    this->minAmount = minimum;
    invalidateFilter();
}

void BlocknetTransactionHistoryFilterProxy::setTypeFilter(quint32 types) {
    this->typeFilter = types;
    invalidateFilter();
}

void BlocknetTransactionHistoryFilterProxy::setDateRange(const QDateTime &from, const QDateTime &to) {
    this->dateFrom = from;
    this->dateTo = to;
    invalidateFilter();
}

bool BlocknetTransactionHistoryFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const {
    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);

    auto type = index.data(TransactionTableModel::TypeRole).toInt();
    if (!(TYPE(type) & typeFilter))
        return false;

    QDateTime datetime = index.data(TransactionTableModel::DateRole).toDateTime();
    QString address = index.data(TransactionTableModel::AddressRole).toString();
    QString label = index.data(TransactionTableModel::LabelRole).toString();
    qint64 amount = llabs(index.data(TransactionTableModel::AmountRole).toLongLong());
    int status = index.data(TransactionTableModel::StatusRole).toInt();

    if (status == TransactionStatus::Conflicted)
        return false;
    if (datetime < dateFrom || datetime > dateTo)
        return false;
    if (!address.contains(addrPrefix, Qt::CaseInsensitive) && !label.contains(addrPrefix, Qt::CaseInsensitive))
        return false;
    if (amount < minAmount)
        return false;

    return true;
}

bool BlocknetTransactionHistoryFilterProxy::lessThan(const QModelIndex &left, const QModelIndex &right) const {
    if (left.column() == BlocknetTransactionHistoryFilterProxy::HistoryDate) {
        QVariant leftData = sourceModel()->index(left.row(), TransactionTableModel::Date).data(Qt::EditRole);
        QVariant rightData = sourceModel()->index(right.row(), TransactionTableModel::Date).data(Qt::EditRole);
        return leftData.toLongLong() < rightData.toLongLong();
    }
    return QSortFilterProxyModel::lessThan(left, right);
}

int BlocknetTransactionHistoryFilterProxy::columnCount(const QModelIndex &) const {
    return BlocknetTransactionHistoryFilterProxy::HistoryAmount + 1;
}

int BlocknetTransactionHistoryFilterProxy::rowCount(const QModelIndex& parent) const {
    if (limitRows != -1)
        return std::min(QSortFilterProxyModel::rowCount(parent), limitRows);
    else
        return QSortFilterProxyModel::rowCount(parent);
}

QVariant BlocknetTransactionHistoryFilterProxy::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal) {
        if (role == Qt::DisplayRole) {
            switch (section) {
                case HistoryStatus:
                    return tr("");
                case HistoryDate:
                    return tr("Date");
                case HistoryTime:
                    return tr("");
                case HistoryToAddress:
                    return tr("Address");
                case HistoryType:
                    return tr("Type");
                case HistoryAmount:
                    return tr("Amount");
                default:
                    return tr("");
            }
        } else if (role == Qt::TextAlignmentRole) {
            switch (section) {
                case HistoryDate:
                    return Qt::AlignCenter;
                default:
                    return Qt::AlignLeft;
            }
        }
    }
    return QSortFilterProxyModel::headerData(section, orientation, role);
}

QVariant BlocknetTransactionHistoryFilterProxy::data(const QModelIndex &index, int role) const {
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
                case HistoryStatus:
                    return static_cast<int>(rec->status.status);
                case HistoryDate:
                    return model->formatTxDate(rec);
                case HistoryTime:
                    return QDateTime::fromTime_t(static_cast<uint>(rec->time)).toString("h:mmap");
                case HistoryToAddress: {
                    auto addr = model->formatTxToAddress(rec, false);
                    if (addr.isEmpty())
                        addr = tr("n/a");
                    return addr;
                }
                case HistoryType:
                    return model->formatTxType(rec);
                case HistoryAmount: {
                    auto amt = static_cast<CAmount>(rec->credit + rec->debit);
                    auto str = BitcoinUnits::floorWithUnit(optionsModel->getDisplayUnit(), amt, true, BitcoinUnits::separatorNever);
                    if (!rec->status.countsForBalance)
                        str = QString("[%1]").arg(str);
                    return str;
                }
            }
            break;
        case Qt::EditRole: // Edit role is used for sorting, so return the unformatted values
        case Qt::UserRole:
            switch (index.column()) {
                case HistoryStatus:
                    return QString::fromStdString(rec->status.sortKey);
                case HistoryDate: {
                    return rec->time;
                }
                case HistoryTime:
                    return rec->time;
                case HistoryType:
                    return model->formatTxType(rec);
                case HistoryToAddress:
                    return model->formatTxToAddress(rec, true);
                case HistoryAmount:
                    return static_cast<qint64>(rec->credit + rec->debit);
            }
            break;
        case Qt::DecorationRole:
            switch (index.column()) {
                case HistoryStatus:
                    return model->txStatusDecoration(rec);
                default:
                    return QVariant();
            }
            break;
        case Qt::ToolTipRole:
            return model->formatTooltip(rec);
        case Qt::TextAlignmentRole:
            switch (index.column()) {
                case HistoryStatus:
                    return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
                case HistoryDate:
                case HistoryTime:
                    return static_cast<int>(Qt::AlignCenter | Qt::AlignVCenter);
                case HistoryType:
                case HistoryToAddress:
                case HistoryAmount:
                default:
                    return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
            }
            break;
        case Qt::ForegroundRole:
            if (index.column() == HistoryAmount) {
                if ((rec->credit + rec->debit) < 0)
                    return QColor(0xFB, 0x7F, 0x70);
                else if ((rec->credit + rec->debit) > 0)
                    return QColor(0x4B, 0xF5, 0xC6);
            }
            return QColor("white");
    }
    return QSortFilterProxyModel::data(index, role);
}

BlocknetTransactionHistoryCellItem::BlocknetTransactionHistoryCellItem(QObject *parent) : QStyledItemDelegate(parent) { }

void BlocknetTransactionHistoryCellItem::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    painter->save();

    switch (index.column()) {
        case BlocknetTransactionHistoryFilterProxy::HistoryStatus: {
            QColor color;
            auto status = static_cast<TransactionStatus::Status>(index.data(Qt::DisplayRole).toInt());
            switch (status) {
                case TransactionStatus::Status::Confirmed:
                case TransactionStatus::Status::Confirming:
                    color.setRgb(0x4B, 0xF5, 0xC6);
                    break;
                case TransactionStatus::Status::OpenUntilDate:
                case TransactionStatus::Status::OpenUntilBlock:
                case TransactionStatus::Status::Offline:
                case TransactionStatus::Status::Unconfirmed:
                case TransactionStatus::Status::Immature:
                    color.setRgb(0xF8, 0xBF, 0x1C);
                    break;
                case TransactionStatus::Status::Conflicted:
                case TransactionStatus::Status::MaturesWarning:
                case TransactionStatus::Status::NotAccepted:
                default:
                    color.setRgb(0xFB, 0x7F, 0x70);
                    break;
            }
            // Draw the status indicator, leave some room on top and bottom
            int pad = 2;
            QRect r(option.rect.x(), option.rect.y()+pad/2, option.rect.width(), option.rect.height()-pad);
            painter->fillRect(r, color);
            break;
        }
        case BlocknetTransactionHistoryFilterProxy::HistoryDate: {
            auto date = QDateTime::fromTime_t(index.data(Qt::EditRole).toULongLong());
            auto month = date.toString("MMM").toUpper();
            auto dt = date.toString("dd");
            painter->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
            if (option.showDecorationSelected && (option.state & QStyle::State_Selected)) {
                QColor color;
                color.setRgb(0x01, 0x6A, 0xFF);
                painter->fillRect(option.rect, color);
            }
            // Draw the month
            painter->save();
            painter->setPen(QColor("white"));
            painter->setFont(QFont("Roboto", 13, 25));
            painter->drawText(QRect(option.rect.x(), option.rect.y() + 5, option.rect.width(), option.rect.height()*0.4), month, Qt::AlignCenter | Qt::AlignVCenter);
            painter->restore();
            // Draw the date
            painter->save();
            painter->setFont(QFont("Roboto", 21, 25));
            painter->setPen(QColor("white"));
            painter->drawText(QRect(option.rect.x(), option.rect.y() + 20, option.rect.width(), option.rect.height()*0.6), dt, Qt::AlignCenter | Qt::AlignVCenter);
            painter->restore();
            break;
        }
    }

    painter->restore();
}

QSize BlocknetTransactionHistoryCellItem::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const {
    switch (index.column()) {
        case BlocknetTransactionHistoryFilterProxy::HistoryStatus:
            return {3, option.rect.height()};
        case BlocknetTransactionHistoryFilterProxy::HistoryDate:
            return {60, option.rect.height()};
    }
    return QStyledItemDelegate::sizeHint(option, index);
}