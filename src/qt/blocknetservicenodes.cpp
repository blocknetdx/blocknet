// Copyright (c) 2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetservicenodes.h>

#include <qt/blocknetcheckbox.h>
#include <qt/blocknetcreateproposal.h>
#include <qt/blocknetdropdown.h>
#include <qt/blocknetguiutil.h>
#include <qt/blocknetformbtn.h>
#include <qt/blocknethdiv.h>
#include <qt/blocknetvars.h>

#include <qt/guiutil.h>
#include <qt/optionsmodel.h>

#include <net.h>
#include <servicenode/servicenodemgr.h>
#include <uint256.h>
#ifdef ENABLE_WALLET
#include <wallet/wallet.h>
#endif // ENABLE_WALLET

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QFormLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QSettings>
#include <QVariant>

BlocknetServiceNodes::BlocknetServiceNodes(QFrame *parent) : QFrame(parent), layout(new QVBoxLayout),
                                                             clientModel(nullptr), contextMenu(new QMenu)
{
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->setContentsMargins(BGU::spi(46), BGU::spi(10), BGU::spi(50), 0);
    this->setLayout(layout);

    titleLbl = new QLabel(tr("Service Nodes"));
    titleLbl->setObjectName("h4");
    titleLbl->setFixedHeight(BGU::spi(26));

    auto *topBox = new QFrame;
    auto *topBoxLayout = new QHBoxLayout;
    topBoxLayout->setContentsMargins(QMargins());
    topBox->setLayout(topBoxLayout);

    filterLbl = new QLabel(tr("Filter by:"));
    filterLbl->setObjectName("title");
    QStringList list{tr("All Service Nodes"), tr("Mine"), tr("Public")};
    filterDd = new BlocknetDropdown(list);

    topBoxLayout->addStretch(1);
    topBoxLayout->addWidget(filterLbl);
    topBoxLayout->addWidget(filterDd);

    table = new QTableWidget;
    table->setContentsMargins(QMargins());
    table->setColumnCount(COLUMN_PADDING + 1);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setFocusPolicy(Qt::NoFocus);
    table->setAlternatingRowColors(true);
    table->setColumnWidth(COLUMN_COLOR, BGU::spi(3));
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
    table->horizontalHeader()->setSectionResizeMode(COLUMN_COLOR, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_ALIAS, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_IP, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_STATUS, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_LASTSEEN, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_PUBKEY, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_SERVICES, QHeaderView::Stretch);
    table->setColumnHidden(COLUMN_PRIVKEY, true);
    table->setHorizontalHeaderLabels({ "", "", tr("Alias"), tr("Host"), tr("Status"), tr("Last Seen"), tr("Pubkey"), tr("Services"), ""});

    auto *titleHBox = new QHBoxLayout;
    titleHBox->setContentsMargins(QMargins());
    titleHBox->addWidget(titleLbl, 0, Qt::AlignLeft);
    titleHBox->addStretch(1);
    titleHBox->addWidget(topBox, 0, Qt::AlignRight);
    layout->addLayout(titleHBox, 0);
    layout->addSpacing(BGU::spi(15));
    layout->addWidget(table);
    layout->addSpacing(BGU::spi(20));

    // context menu
    auto *viewDetails = new QAction(tr("View Details"), this);
    auto *copyPubKey = new QAction(tr("Copy Public Key"), this);
    auto *copyPrivKey = new QAction(tr("Copy Private Key"), this);
    auto *registerSnode = new QAction(tr("Register Service Node"), this);
    auto *pingSnode = new QAction(tr("Send Ping"), this);
    contextMenu->addAction(viewDetails);
    contextMenu->addSeparator();
    contextMenu->addAction(copyPubKey);
    contextMenu->addAction(copyPrivKey);
    contextMenu->addSeparator();
    contextMenu->addAction(registerSnode);
//    contextMenu->addAction(pingSnode);

    connect(table, &QTableWidget::itemSelectionChanged, this, [this]() {
        lastSelection = QDateTime::currentMSecsSinceEpoch();
    });
    connect(table, &QTableWidget::itemClicked, this, [this](QTableWidgetItem *item) {
        if (item && item->row() == lastRow && (QDateTime::currentMSecsSinceEpoch() - lastSelection > 250)) // defeat event loop w/ time check
            table->clearSelection();
        auto items = table->selectedItems();
        if (items.count() > 0)
            lastRow = items[0]->row();
        else lastRow = -1;
    });
    connect(table, &QTableWidget::cellDoubleClicked, this, [this](int row, int column) {
        if (row >= filteredData.size())
            return;
        auto *pubkeyItem = table->item(row, COLUMN_PUBKEY);
        if (pubkeyItem)
            showServiceNodeDetails(serviceNodeForPubkey(pubkeyItem->data(Qt::DisplayRole).toString().toStdString()));
    });
    connect(table, &QTableWidget::customContextMenuRequested, this, &BlocknetServiceNodes::showContextMenu);
    connect(filterDd, &BlocknetDropdown::valueChanged, this, &BlocknetServiceNodes::onFilter);

    connect(viewDetails, &QAction::triggered, this, [this]() {
        if (contextItem == nullptr || contextItem->row() >= filteredData.size())
            return;
        auto *pubkeyItem = table->item(contextItem->row(), COLUMN_PUBKEY);
        if (pubkeyItem)
            showServiceNodeDetails(serviceNodeForPubkey(pubkeyItem->data(Qt::DisplayRole).toString().toStdString()));
    });
    connect(copyPubKey, &QAction::triggered, this, [this]() {
        if (contextItem == nullptr)
            return;
        auto *pubkeyItem = table->item(contextItem->row(), COLUMN_PUBKEY);
        if (pubkeyItem)
            QApplication::clipboard()->setText(pubkeyItem->data(Qt::DisplayRole).toString(), QClipboard::Clipboard);
    });
    connect(copyPrivKey, &QAction::triggered, this, [this]() {
        if (contextItem == nullptr)
            return;
        auto *pubkeyItem = table->item(contextItem->row(), COLUMN_PUBKEY);
        if (pubkeyItem) {
            const std::string & pubkeyhex = pubkeyItem->data(Qt::DisplayRole).toString().toStdString();
            const auto & pk = ParseHex(pubkeyhex);
            CPubKey pubkey(pk);
            if (!pubkey.IsFullyValid()) {
                QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Unable to copy the private key because it's invalid"));
                return;
            }
            const auto & entry = sn::ServiceNodeMgr::instance().getSnEntry(pubkey.GetID());
            if (entry.isNull()) {
                QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Unable to copy the private key because this service node is not yours"));
                return;
            }
            auto *privkeyItem = table->item(contextItem->row(), COLUMN_PRIVKEY);
            auto key = DecodeSecret(privkeyItem->data(Qt::DisplayRole).toString().toStdString());
            if (!key.IsValid()) {
                QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Unable to copy the private key because it's invalid"));
                return;
            }
            QApplication::clipboard()->setText(privkeyItem->data(Qt::DisplayRole).toString(), QClipboard::Clipboard);
        }
    });
    connect(registerSnode, &QAction::triggered, this, [this]() {
        if (contextItem == nullptr)
            return;
        auto *pubkeyItem = table->item(contextItem->row(), COLUMN_PUBKEY);
        if (pubkeyItem) {
#ifdef ENABLE_WALLET
            const std::string & pubkeyhex = pubkeyItem->data(Qt::DisplayRole).toString().toStdString();
            const auto & pk = ParseHex(pubkeyhex);
            CPubKey pubkey(pk);
            const auto & entry = sn::ServiceNodeMgr::instance().getSnEntry(pubkey.GetID());
            if (entry.isNull()) {
                QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Unable to register this service node because it is not yours"));
                return;
            }
            std::string failReason;
            if (!sn::ServiceNodeMgr::instance().registerSn(entry, g_connman.get(), GetWallets(), &failReason)) {
                QMessageBox::warning(this->parentWidget(), tr("Issue"),
                        tr("Failed to register: %1").arg(QString::fromStdString(failReason)));
            }
            return;
#else
            QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Unable to register the service node because the wallet is disabled"));
#endif // ENABLE_WALLET
        }
    });
//    connect(pingSnode, &QAction::triggered, this, [this]() {
//        if (contextItem == nullptr)
//            return;
//        if (!sn::ServiceNodeMgr::instance().hasActiveSn()) {
//            QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Failed to send ping. You must send the ping from the Service Node client."));
//            return;
//        }
//        auto *pubkeyItem = table->item(contextItem->row(), COLUMN_PUBKEY);
//        if (pubkeyItem) {
//            const std::string & pubkeyhex = pubkeyItem->data(Qt::DisplayRole).toString().toStdString();
//            const auto & pk = ParseHex(pubkeyhex);
//            CPubKey pubkey(pk);
//            if (!sn::ServiceNodeMgr::instance().getSnEntry(pubkey.GetID()).isNull()) {
//                xbridge::App & xapp = xbridge::App::instance();
//                if (!sn::ServiceNodeMgr::instance().sendPing(XROUTER_PROTOCOL_VERSION, xapp.myServicesJSON(), g_connman.get())) {
//                    QMessageBox::warning(this->parentWidget(), tr("Issue"),
//                                         tr("Failed to send ping: %1").arg(QString::fromStdString(failReason)));
//                }
//                return;
//            }
//        }
//    });

    QSettings settings;
    auto filterIdx = settings.value("serviceNodeFilter", FILTER_ALL);
    filterDd->setCurrentIndex(filterIdx.toInt());
}

void BlocknetServiceNodes::initialize() {
    dataModel.clear();

    const auto & snodes = sn::ServiceNodeMgr::instance().list();
    for (const auto & snode : snodes)
        dataModel << snode;

    // Add any of our snode entries that aren't registered with the network
    const auto & entries = sn::ServiceNodeMgr::instance().getSnEntries();
    for (const auto & entry : entries) {
        if (!sn::ServiceNodeMgr::instance().getSn(entry.key.GetPubKey()).isNull())
            continue;
        sn::ServiceNode snode(entry.key.GetPubKey(), entry.tier, CKeyID{}, std::vector<COutPoint>{}, 0, uint256{},
                std::vector<unsigned char>{});
        dataModel << snode;
    }

    this->setData(dataModel);
}

void BlocknetServiceNodes::setClientModel(ClientModel *c) {
    if (clientModel == c)
        return;

    if (clientModel)
        disconnect(clientModel, &ClientModel::numBlocksChanged, this, &BlocknetServiceNodes::setNumBlocks);

    clientModel = c;
    if (!clientModel)
        return;
    connect(clientModel, &ClientModel::numBlocksChanged, this, &BlocknetServiceNodes::setNumBlocks);

    initialize();
    onFilter();
}

void BlocknetServiceNodes::setData(const QVector<sn::ServiceNode> & data) {
    this->filteredData = data;

    unwatch();
    table->clearContents();
    table->setRowCount(this->filteredData.count());
    table->setSortingEnabled(false);

    for (int i = 0; i < this->filteredData.count(); ++i) {
        const auto & snode = this->filteredData[i];
        const sn::ServiceNodeConfigEntry & entry = sn::ServiceNodeMgr::instance().getSnEntry(snode.getSnodePubKey().GetID());

        // color indicator
        auto *colorItem = new QTableWidgetItem;
        auto *indicatorBox = new QFrame;
        indicatorBox->setObjectName("indicator");
        indicatorBox->setContentsMargins(QMargins());
        indicatorBox->setProperty("state", QVariant(snode.running() ? 0 : 2)); // 0 is green, 2 is red
        indicatorBox->setFixedWidth(BGU::spi(3));
        table->setCellWidget(i, COLUMN_COLOR, indicatorBox);
        table->setItem(i, COLUMN_COLOR, colorItem);

        // snode alias
        auto *aliasItem = new QTableWidgetItem;
        aliasItem->setData(Qt::DisplayRole, entry.alias.empty() ? QString("[%1]").arg(tr("Public")) : QString::fromStdString(entry.alias));
        table->setItem(i, COLUMN_ALIAS, aliasItem);

        // ip/dns
        auto *ipItem = new QTableWidgetItem;
        ipItem->setData(Qt::DisplayRole, QString::fromStdString(snode.getHost()));
        table->setItem(i, COLUMN_IP, ipItem);

        // status
        auto *statusItem = new QTableWidgetItem;
        statusItem->setData(Qt::DisplayRole, snode.running() ? "running" : "offline");
        table->setItem(i, COLUMN_STATUS, statusItem);

        // last seen
        auto *seenItem = new QTableWidgetItem;
        seenItem->setData(Qt::DisplayRole, QString::fromStdString(FormatISO8601DateTime(snode.getPingTime())));
        table->setItem(i, COLUMN_LASTSEEN, seenItem);

        // pubkey
        auto *pubkeyItem = new QTableWidgetItem;
        pubkeyItem->setData(Qt::DisplayRole, QString::fromStdString(HexStr(snode.getSnodePubKey())));
        table->setItem(i, COLUMN_PUBKEY, pubkeyItem);

        // services
        auto *servicesItem = new QTableWidgetItem;
        const auto & services = snode.serviceList();
        servicesItem->setData(Qt::DisplayRole, services.empty() ? QString("[%1]").arg(tr("missing")) : QString::fromStdString(boost::algorithm::join(services, ",")));
        table->setItem(i, COLUMN_SERVICES, servicesItem);

        // priv key (hidden)
        auto *privkeyItem = new QTableWidgetItem;
        privkeyItem->setData(Qt::DisplayRole, QString::fromStdString(entry.isNull() ? "" : EncodeSecret(entry.key)));
        table->setItem(i, COLUMN_PRIVKEY, privkeyItem);
    }

    table->setSortingEnabled(true);
    watch();
}

QVector<sn::ServiceNode> BlocknetServiceNodes::filtered(const int & filter) {
    QVector<sn::ServiceNode> r;
    for (const auto & snode : dataModel) {
        switch (filter) {
            case FILTER_MINE: {
                sn::ServiceNodeConfigEntry entry;
                if (sn::ServiceNodeMgr::instance().isMine(snode, entry))
                    r.push_back(snode);
                break;
            }
            case FILTER_OTHER: {
                sn::ServiceNodeConfigEntry entry;
                if (!sn::ServiceNodeMgr::instance().isMine(snode, entry))
                    r.push_back(snode);
                break;
            }
            case FILTER_RUNNING: {
                if (snode.running())
                    r.push_back(snode);
                break;
            }
            case FILTER_OFFLINE: {
                if (!snode.running())
                    r.push_back(snode);
                break;
            }
            case FILTER_ALL:
            default:
                r.push_back(snode);
                break;
        }
    }
    return r;
}

/**
 * @brief Refreshes the display if necessary.
 * @param force Set true to force a refresh (bypass all checks).
 */
void BlocknetServiceNodes::refresh(bool force) {
    if (!force && dataModel.size() == static_cast<int>(sn::ServiceNodeMgr::instance().list().size())) // ignore if the list hasn't changed
        return;
    initialize();
    onFilter();
}

/**
 * @brief Filters the data model based on the current filter dropdown filter flag.
 */
void BlocknetServiceNodes::onFilter() {
    setData(filtered(filterDd->currentIndex()));
    QSettings settings;
    settings.setValue("serviceNodeFilter", filterDd->currentIndex());
}

void BlocknetServiceNodes::onItemChanged(QTableWidgetItem *item) {
    if (dataModel.count() > item->row())
        Q_EMIT tableUpdated();
}

void BlocknetServiceNodes::unwatch() {
    table->setEnabled(false);
    disconnect(table, &QTableWidget::itemChanged, this, &BlocknetServiceNodes::onItemChanged);
}

void BlocknetServiceNodes::watch() {
    table->setEnabled(true);
    connect(table, &QTableWidget::itemChanged, this, &BlocknetServiceNodes::onItemChanged);
}

void BlocknetServiceNodes::showContextMenu(QPoint pt) {
    auto *item = table->itemAt(pt);
    if (!item) {
        contextItem = nullptr;
        return;
    }
    contextItem = item;
    contextMenu->exec(QCursor::pos());
}

void BlocknetServiceNodes::showServiceNodeDetails(const sn::ServiceNode & snode) {
    auto *dialog = new BlocknetServiceNodeDetailsDialog(snode);
    dialog->setStyleSheet(GUIUtil::loadStyleSheet());
    dialog->exec();
}

void BlocknetServiceNodes::setNumBlocks(int count, const QDateTime & blockDate, double nVerificationProgress, bool header) {
    // Only refresh data if the cache changes
    const auto newSnodeCount = static_cast<int>(sn::ServiceNodeMgr::instance().list().size());
    // refresh data if necessary
    if (lastKnownSnodeCount != newSnodeCount) {
        lastKnownSnodeCount = newSnodeCount;
        initialize();
        onFilter();
    }
}

sn::ServiceNode BlocknetServiceNodes::serviceNodeForPubkey(const std::string & hex) {
    for (const auto & snode : filteredData) {
        if (HexStr(snode.getSnodePubKey()) == hex)
            return snode;
    }
    return sn::ServiceNode{};
}

/**
 * Dialog that shows proposal information and user votes.
 * @param snode
 * @param parent Optional parent widget to attach to
 */
BlocknetServiceNodeDetailsDialog::BlocknetServiceNodeDetailsDialog(const sn::ServiceNode & snode, QWidget *parent) : QDialog(parent) {
    auto *layout = new QVBoxLayout;
    layout->setContentsMargins(BGU::spi(30), BGU::spi(10), BGU::spi(30), BGU::spi(10));
    this->setLayout(layout);
    this->setMinimumWidth(BGU::spi(550));
    this->setWindowTitle(tr("Service Node Details"));

    sn::ServiceNodeConfigEntry entry;
    sn::ServiceNodeMgr::instance().isMine(snode, entry);
    auto *titleLbl = new QLabel(tr("%1").arg(entry.isNull() ? tr("Public") : QString::fromStdString(entry.alias)));
    titleLbl->setObjectName("h2");
    titleLbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    auto *descBox = new QFrame;
    descBox->setContentsMargins(QMargins());
    descBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
    auto *descBoxLayout = new QGridLayout;
    descBoxLayout->setContentsMargins(QMargins());
    descBox->setLayout(descBoxLayout);

    auto alias = entry.alias.empty() ? tr("Public") : QString::fromStdString(entry.alias);
    auto tier = QString::fromStdString(sn::ServiceNodeMgr::tierString(snode.getTier()));
    auto pubkey = QString::fromStdString(HexStr(snode.getSnodePubKey()));
    auto address = QString::fromStdString(EncodeDestination(CTxDestination(snode.getPaymentAddress())));
    auto lastseen = QString::number(snode.getPingTime());
    auto lastseenstr = QString::fromStdString(FormatISO8601DateTime(snode.getPingTime()));
    auto status = snode.running() ? tr("running") : tr("offline");
    auto ip = QString::fromStdString(snode.getHost());
    auto services = QString::fromStdString(boost::algorithm::join(snode.serviceList(), ", "));

    auto *aliasLbl       = new QLabel(tr("Alias"));           aliasLbl->setObjectName("description");
    auto *aliasVal       = new QLabel(alias);                 aliasVal->setObjectName("value");              aliasVal->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *tierLbl        = new QLabel(tr("Tier"));            tierLbl->setObjectName("description");
    auto *tierVal        = new QLabel(tier);                  tierVal->setObjectName("value");
    auto *snodekeyLbl    = new QLabel(tr("Public Key"));      snodekeyLbl->setObjectName("description");
    auto *snodekeyVal    = new QLabel(pubkey);                snodekeyVal->setObjectName("value");           snodekeyVal->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *addressLbl     = new QLabel(tr("Payment Address")); addressLbl->setObjectName("description");
    auto *addressVal     = new QLabel(address);               addressVal->setObjectName("value");            addressVal->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *lastSeenStrLbl = new QLabel(tr("Last Updated"));    lastSeenStrLbl->setObjectName("description");
    auto *lastSeenStrVal = new QLabel(lastseenstr);           lastSeenStrVal->setObjectName("value");
    auto *statusLbl      = new QLabel(tr("Status"));          statusLbl->setObjectName("description");
    auto *statusVal      = new QLabel(status);                statusVal->setObjectName("value");
    auto *ipLbl          = new QLabel(tr("Host"));            ipLbl->setObjectName("description");
    auto *ipVal          = new QLabel(ip);                    ipVal->setObjectName("value");
    auto *servicesLbl    = new QLabel(tr("Services"));        servicesLbl->setObjectName("description");
    auto *servicesVal    = new QLabel(services);              servicesVal->setObjectName("value");           servicesVal->setWordWrap(true); servicesVal->setTextInteractionFlags(Qt::TextSelectableByMouse);
    servicesVal->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QVector<QLabel*> labels{aliasLbl,tierLbl,snodekeyLbl,addressLbl,lastSeenStrLbl,statusLbl,ipLbl,servicesLbl};
    QVector<QLabel*> values{aliasVal,tierVal,snodekeyVal,addressVal,lastSeenStrVal,statusVal,ipVal,servicesVal};
    auto *pl = new QLabel;
    for (int i = 0; i < labels.size(); ++i) {
        descBoxLayout->addWidget(labels[i], i, 0, Qt::AlignTop); // column 1
        descBoxLayout->addWidget(values[i], i, 1, Qt::AlignTop); // column 2
        descBoxLayout->addWidget(pl, i, 2);                      // column 3
        descBoxLayout->setRowMinimumHeight(i, BGU::spi(22));     // row height
    }
    descBoxLayout->setRowStretch(labels.size() - 1, 1); // sevices are last

    auto *btnBox = new QFrame;
    btnBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
    auto *btnBoxLayout = new QHBoxLayout;
    btnBoxLayout->setContentsMargins(QMargins());
    btnBoxLayout->setSpacing(BGU::spi(15));
    btnBox->setLayout(btnBoxLayout);
    auto *closeBtn = new BlocknetFormBtn;
    closeBtn->setText(tr("Close"));
    btnBoxLayout->addWidget(closeBtn, 0, Qt::AlignCenter);

    layout->addSpacing(BGU::spi(5));
    layout->addWidget(titleLbl);
    layout->addSpacing(BGU::spi(10));
    layout->addWidget(descBox);
    layout->addSpacing(BGU::spi(10));
    layout->addWidget(new BlocknetHDiv);
    layout->addSpacing(BGU::spi(10));
    layout->addStretch(1);
    layout->addWidget(btnBox);
    layout->addSpacing(BGU::spi(10));

    connect(closeBtn, &BlocknetFormBtn::clicked, this, [this]() {
        close();
    });
}
