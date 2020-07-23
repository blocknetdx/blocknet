// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetcoincontrol.h>

#include <qt/addresstablemodel.h>
#include <qt/bitcoinunits.h>
#include <qt/optionsmodel.h>

#include <key_io.h>
#include <uint256.h>

#include <QApplication>
#include <QAbstractItemView>
#include <QButtonGroup>
#include <QClipboard>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QResizeEvent>
#include <QSettings>
#include <QSizePolicy>

/**
 * @brief Dialog encapsulates the coin control table. The default size is 960x580
 * @param parent
 */
BlocknetCoinControlDialog::BlocknetCoinControlDialog(WalletModel *w, QWidget *parent, Qt::WindowFlags f, bool standaloneMode) : 
    QDialog(parent, f), walletModel(w), standaloneMode(standaloneMode) 
{
    //this->setStyleSheet("border: 1px solid red;");
    this->setContentsMargins(QMargins());
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setMinimumSize(BGU::spi(400), BGU::spi(250));
    this->setWindowTitle(tr("Coin Control"));

    content = new QFrame(this);
    content->setObjectName("coinControlDialog");
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *contentLayout = new QVBoxLayout;
    contentLayout->setContentsMargins(0, 0, 0, BGU::spi(20));
    content->setLayout(contentLayout);

    confirmBtn = new BlocknetFormBtn;
    confirmBtn->setText(tr("Confirm"));
    cancelBtn = new BlocknetFormBtn;
    cancelBtn->setObjectName("cancel");
    cancelBtn->setText(standaloneMode ? tr("Close") : tr("Cancel"));

    auto *btnBox = new QFrame;
    auto *btnBoxLayout = new QHBoxLayout;
    btnBoxLayout->setContentsMargins(QMargins());
    btnBoxLayout->setSpacing(BGU::spi(15));
    btnBox->setLayout(btnBoxLayout);
    btnBoxLayout->addStretch(1);
    btnBoxLayout->addWidget(cancelBtn, 0, Qt::AlignLeft);
    if (!standaloneMode)
        btnBoxLayout->addWidget(confirmBtn, 0, Qt::AlignLeft);
    btnBoxLayout->addStretch(1);

    // Manages the coin list
    cc = new BlocknetCoinControl(nullptr, w);

    // Manages the selected coin details
    feePanel = new QFrame;
    feePanel->setObjectName("feePanel");
    feePanel->setContentsMargins(BGU::spi(20), 0, BGU::spi(20), 0);
    feePanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    feePanelLayout = new QGridLayout;
    feePanelLayout->setHorizontalSpacing(BGU::spi(30));
    feePanel->setLayout(feePanelLayout);
    feePanel->setHidden(true);

    // row 1
    quantityLbl = new QLabel(QString("%1:").arg(tr("Selected Inputs")));
    quantityVal = new QLabel;                                       quantityVal->setTextInteractionFlags(Qt::TextSelectableByMouse);
    amountLbl = new QLabel(QString("%1:").arg(tr("Amount")));
    amountVal = new QLabel;                                         amountVal->setTextInteractionFlags(Qt::TextSelectableByMouse);
    feeLbl = new QLabel(QString("%1:").arg(tr("Estimated Fee")));
    feeVal = new QLabel;                                            feeVal->setTextInteractionFlags(Qt::TextSelectableByMouse);
    afterFeeLbl = new QLabel(QString("%1:").arg(tr("After Fee")));
    afterFeeVal = new QLabel;                                       afterFeeVal->setTextInteractionFlags(Qt::TextSelectableByMouse);
    feePanelLayout->addWidget(quantityLbl, 0, 0, Qt::AlignRight);
    feePanelLayout->addWidget(quantityVal, 0, 1, Qt::AlignLeft);
    feePanelLayout->addWidget(amountLbl, 0, 2, Qt::AlignRight);
    feePanelLayout->addWidget(amountVal, 0, 3, Qt::AlignLeft);
    feePanelLayout->addWidget(feeLbl, 0, 4, Qt::AlignRight);
    feePanelLayout->addWidget(feeVal, 0, 5, Qt::AlignLeft);
    feePanelLayout->addWidget(afterFeeLbl, 0, 6, Qt::AlignRight);
    feePanelLayout->addWidget(afterFeeVal, 0, 7, Qt::AlignLeft);

    // row 2
    bytesLbl = new QLabel(QString("%1:").arg(tr("Bytes")));
    bytesVal = new QLabel;                                          bytesVal->setTextInteractionFlags(Qt::TextSelectableByMouse);
    priorityLbl = new QLabel(QString("%1:").arg(tr("Priority")));
    priorityVal = new QLabel;                                       priorityVal->setTextInteractionFlags(Qt::TextSelectableByMouse);
    dustLbl = new QLabel(QString("%1:").arg(tr("Dust")));
    dustVal = new QLabel;                                           dustVal->setTextInteractionFlags(Qt::TextSelectableByMouse);
    changeLbl = new QLabel(QString("%1:").arg(tr("Change")));
    changeVal = new QLabel;                                         changeVal->setTextInteractionFlags(Qt::TextSelectableByMouse);
    feePanelLayout->addWidget(bytesLbl, 1, 0, Qt::AlignRight);
    feePanelLayout->addWidget(bytesVal, 1, 1, Qt::AlignLeft);
    feePanelLayout->addWidget(priorityLbl, 1, 2, Qt::AlignRight);
    feePanelLayout->addWidget(priorityVal, 1, 3, Qt::AlignLeft);
    feePanelLayout->addWidget(dustLbl, 1, 4, Qt::AlignRight);
    feePanelLayout->addWidget(dustVal, 1, 5, Qt::AlignLeft);
    feePanelLayout->addWidget(changeLbl, 1, 6, Qt::AlignRight);
    feePanelLayout->addWidget(changeVal, 1, 7, Qt::AlignLeft);

    feePanelLayout->setRowMinimumHeight(0, BGU::spi(20));
    feePanelLayout->setRowMinimumHeight(1, BGU::spi(20));

    contentLayout->addWidget(cc, 1);
    contentLayout->addSpacing(5);
    contentLayout->addWidget(feePanel);
    contentLayout->addWidget(btnBox);

    this->resize(BGU::spi(1010), BGU::spi(680));

    connect(confirmBtn, &QPushButton::clicked, this, [this]() {
        Q_EMIT accept();
    });
    connect(cancelBtn, &QPushButton::clicked, this, [this]() {
        cc->setData(std::make_shared<BlocknetCoinControl::Model>());
        Q_EMIT reject();
    });
    connect(cc, &BlocknetCoinControl::tableUpdated, this, &BlocknetCoinControlDialog::updateLabels);

    updateLabels();
}

void BlocknetCoinControlDialog::resizeEvent(QResizeEvent *evt) {
    QDialog::resizeEvent(evt);
    content->resize(evt->size().width(), evt->size().height());
}

void BlocknetCoinControlDialog::showEvent(QShowEvent *event) {
    QDialog::showEvent(event);
    updateLabels();
}

void BlocknetCoinControlDialog::updateLabels() {
    // TODO Blocknet Qt handle fee info
    feePanel->setHidden(true);
//    auto displayUnit = walletModel->getOptionsModel()->getDisplayUnit();
//    int64_t totalSelectedAmount = 0;
//    QVector<WalletModel::CoinInput> inputs;
//
//    if (cc->getData() != nullptr) {
//        for (auto &utxo : cc->getData()->data) {
//            if (utxo->unlocked && utxo->checked) {
//                totalSelectedAmount += utxo->camount;
//                inputs.push_back({ uint256S(utxo->transaction.toStdString()), utxo->vout, utxo->address, utxo->camount });
//            }
//        }
//    }
//
//    if (totalSelectedAmount > 0) {
//        auto res = walletModel->wallet().getFeeInfo(inputs, payAmount);
//        quantityVal->setText(QString::number(res.quantity));
//        amountVal->setText(BitcoinUnits::formatWithUnit(displayUnit, res.amount));
//        feeVal->setText(BitcoinUnits::formatWithUnit(displayUnit, res.fee));
//        afterFeeVal->setText(BitcoinUnits::formatWithUnit(displayUnit, res.afterFee));
//        bytesVal->setText(QString::number(res.bytes));
//        priorityVal->setText(cc->getPriorityLabel(res.priority));
//        dustVal->setText(res.dust ? tr("yes") : tr("no"));
//        changeVal->setText(BitcoinUnits::formatWithUnit(displayUnit, res.change));
//    }
//
//    feePanel->setHidden(totalSelectedAmount == 0);
}

void BlocknetCoinControlDialog::populateUnspentTransactions(const QVector<BlocknetSimpleUTXO> & txSelectedUtxos) {
    int displayUnit = walletModel->getOptionsModel()->getDisplayUnit();
    QVector<BlocknetCoinControl::UTXO*> utxos;

    auto mapCoins = walletModel->wallet().listCoins();
    for (auto & item : mapCoins) {
        CAmount nSum = 0;
        double dPrioritySum = 0;
        int nChildren = 0;
        int nInputSum = 0;
        const auto sWalletAddress = EncodeDestination(item.first);

        for (auto & tup : item.second) {
            const auto & out = std::get<0>(tup);
            const auto & walletTx = std::get<1>(tup);
            int nInputSize = 0;
            nSum += walletTx.txout.nValue;
            nChildren++;

            auto *utxo = new BlocknetCoinControl::UTXO;
            utxo->checked = false;

            // address
            CTxDestination outputAddress;
            QString sAddress = "";
            if (ExtractDestination(walletTx.txout.scriptPubKey, outputAddress)) {
                sAddress = QString::fromStdString(EncodeDestination(outputAddress));
                utxo->address = sAddress;
                CPubKey pubkey;
                CKeyID *keyid = boost::get<CKeyID>(&outputAddress);
                if (keyid && walletModel->wallet().getPubKey(*keyid, pubkey) && !pubkey.IsCompressed())
                    nInputSize = 29; // 29 = 180 - 151 (public key is 180 bytes, priority free area is 151 bytes)
            }

            // label
            if (!(sAddress.toStdString() == sWalletAddress)) { // if change
                utxo->label = tr("(change)");
            } else {
                QString sLabel = walletModel->getAddressTableModel()->labelForAddress(sAddress);
                if (sLabel.isEmpty())
                    sLabel = tr("(no label)");
                utxo->label = sLabel;
            }

            // amount
            utxo->amount = BitcoinUnits::format(displayUnit, walletTx.txout.nValue);
            utxo->camount = walletTx.txout.nValue;

            // date
            utxo->date = QDateTime::fromTime_t(static_cast<uint>(walletTx.time));

            // confirmations
            utxo->confirmations = walletTx.depth_in_main_chain;

            // priority
            double dPriority = ((double)walletTx.txout.nValue / (nInputSize + 78)) * (walletTx.depth_in_main_chain + 1); // 78 = 2 * 34 + 10
            utxo->priority = dPriority;
            dPrioritySum += (double)walletTx.txout.nValue * (walletTx.depth_in_main_chain + 1);
            nInputSum += nInputSize;

            // transaction hash & vout
            uint256 txhash = out.hash;
            utxo->transaction = QString::fromStdString(txhash.GetHex());
            utxo->vout = static_cast<unsigned int>(out.n);

            // locked coins
            utxo->locked = walletModel->wallet().isLockedCoin(out);
            utxo->unlocked = !utxo->locked;

            // selected coins
            for (auto &outpt : txSelectedUtxos) {
                if (outpt.hash == txhash && outpt.vout == static_cast<uint>(out.n) && !utxo->locked) {
                    utxo->checked = true;
                    break;
                }
            }

            utxos.push_back(utxo);
        }
    }

    auto ccData = std::make_shared<BlocknetCoinControl::Model>();
    ccData->freeThreshold = COIN * 576 / 250; // TODO Blocknet Qt handle free threshold
    ccData->data = utxos;
    getCC()->setData(ccData);
    
    if (standaloneMode) // only process utxo state changes in standalone mode
        connect(getCC(), &BlocknetCoinControl::tableUpdated, this, &BlocknetCoinControlDialog::updateUTXOState);
}

void BlocknetCoinControlDialog::updateUTXOState() {
    for (auto *data : getCC()->getData()->data) {
        if (data->locked) {
            COutPoint utxo(uint256S(data->transaction.toStdString()), data->vout);
            walletModel->wallet().lockCoin(utxo);
        }
        if (data->unlocked) {
            COutPoint utxo(uint256S(data->transaction.toStdString()), data->vout);
            walletModel->wallet().unlockCoin(utxo);
        }
    }
}

/**
 * @brief Manages and displays the coin control input list.
 * @param parent
 * @param w Wallet model
 */
BlocknetCoinControl::BlocknetCoinControl(QWidget *parent, WalletModel *w) : QFrame(parent), walletModel(w), layout(new QVBoxLayout),
    table(new BlocknetTableWidget), tree(new QTreeWidget), contextMenu(new QMenu)
{
    // this->setStyleSheet("border: 1px solid red");
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->setContentsMargins(QMargins());
    this->setLayout(layout);

    // table
    table->setContentsMargins(QMargins());
    table->setColumnCount(COLUMN_TXVOUT + 1);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->setAlternatingRowColors(true);
    table->setColumnWidth(COLUMN_PADDING1, BGU::spi(1));
    table->setColumnWidth(COLUMN_PADDING2, BGU::spi(1));
    table->setColumnWidth(COLUMN_PADDING3, BGU::spi(1));
    table->setColumnWidth(COLUMN_PADDING4, BGU::spi(1));
    table->setColumnWidth(COLUMN_PADDING5, BGU::spi(1));
    table->setColumnWidth(COLUMN_PADDING6, BGU::spi(1));
    table->setShowGrid(false);
    table->setFocusPolicy(Qt::ClickFocus);
    table->setContextMenuPolicy(Qt::CustomContextMenu);
    table->setColumnHidden(COLUMN_TXHASH, true);
    table->setColumnHidden(COLUMN_TXVOUT, true);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->verticalHeader()->setDefaultSectionSize(BGU::spi(25));
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    table->horizontalHeader()->setSortIndicatorShown(true);
    table->horizontalHeader()->setSectionsClickable(true);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_PADDING1, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_PADDING2, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_PADDING3, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_PADDING4, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_PADDING5, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_PADDING6, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_CHECKBOX, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_AMOUNT, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(COLUMN_ADDRESS, QHeaderView::ResizeToContents);
    table->setHorizontalHeaderLabels({ "", "", "", tr("Amount"), "", tr("Label"), "", tr("Address"), "", tr("Date"), "", tr("Confirmations"), "" });

    // tree
    tree->setContentsMargins(QMargins());
    tree->setColumnCount(COLUMN_TXVOUT + 1);
    tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tree->setAlternatingRowColors(true);
    tree->setColumnWidth(COLUMN_PADDING1, BGU::spi(1));
    tree->setColumnWidth(COLUMN_PADDING2, BGU::spi(1));
    tree->setColumnWidth(COLUMN_PADDING3, BGU::spi(1));
    tree->setColumnWidth(COLUMN_PADDING4, BGU::spi(1));
    tree->setColumnWidth(COLUMN_PADDING5, BGU::spi(1));
    tree->setColumnWidth(COLUMN_PADDING6, BGU::spi(1));
    tree->setFocusPolicy(Qt::ClickFocus);
    tree->setContextMenuPolicy(Qt::CustomContextMenu);
    tree->setColumnHidden(COLUMN_TXHASH, true);
    tree->setColumnHidden(COLUMN_TXVOUT, true);
    tree->header()->setDefaultAlignment(Qt::AlignLeft);
    tree->header()->setSortIndicatorShown(true);
    tree->header()->setSectionsClickable(true);
    tree->header()->setSectionResizeMode(QHeaderView::Stretch);
    tree->header()->setSectionResizeMode(COLUMN_PADDING1, QHeaderView::Fixed);
    tree->header()->setSectionResizeMode(COLUMN_PADDING2, QHeaderView::Fixed);
    tree->header()->setSectionResizeMode(COLUMN_PADDING3, QHeaderView::Fixed);
    tree->header()->setSectionResizeMode(COLUMN_PADDING4, QHeaderView::Fixed);
    tree->header()->setSectionResizeMode(COLUMN_PADDING5, QHeaderView::Fixed);
    tree->header()->setSectionResizeMode(COLUMN_PADDING6, QHeaderView::Fixed);
    tree->header()->setSectionResizeMode(COLUMN_CHECKBOX, QHeaderView::ResizeToContents);
    tree->header()->setSectionResizeMode(COLUMN_AMOUNT, QHeaderView::ResizeToContents);
    tree->header()->setSectionResizeMode(COLUMN_ADDRESS, QHeaderView::ResizeToContents);
    tree->setUniformRowHeights(true);
    tree->setItemDelegate(new TreeDelegate);
    tree->setHeaderLabels({ "", "", "", tr("Amount"), "", tr("Label"), "", tr("Address"), "", tr("Date"), "", tr("Confirmations"), "" });

    // Tree mode
    auto *treeBox = new QFrame;
    auto *treeBoxLayout = new QHBoxLayout;
    treeBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    treeBox->setContentsMargins(QMargins());
    treeBox->setLayout(treeBoxLayout);
    treeBoxLayout->setSpacing(BGU::spi(20));
    listRb = new QRadioButton(tr("List mode"));
    treeRb = new QRadioButton(tr("Tree mode"));
    treeBoxLayout->addStretch(1);
    treeBoxLayout->addWidget(listRb);
    treeBoxLayout->addWidget(treeRb);
    treeBoxLayout->addStretch(1);
    auto *group = new QButtonGroup;
    group->addButton(listRb, 0);
    group->addButton(treeRb, 1);
    group->setExclusive(true);

    // context menu actions
    selectCoins = new QAction(tr("Select coins"), this);
    deselectCoins = new QAction(tr("Deselect coins"), this);
    selectCoins->setEnabled(false);
    deselectCoins->setEnabled(false);
    selectAllCoins = new QAction(tr("Select all coins"), this);
    deselectAllCoins = new QAction(tr("Deselect all coins"), this);
    copyAmountAction = new QAction(tr("Copy amount"), this);
    copyLabelAction = new QAction(tr("Copy label"), this);
    copyAddressAction = new QAction(tr("Copy address"), this);
    copyTransactionAction = new QAction(tr("Copy transaction ID"), this);
    lockAction = new QAction(tr("Lock unspent"), this);
    unlockAction = new QAction(tr("Unlock unspent"), this);
    expandAll = new QAction(tr("Expand all"), this);
    collapseAll = new QAction(tr("Collapse all"), this);
    expandAll->setEnabled(false);
    collapseAll->setEnabled(false);

    // context menu
    contextMenu->addAction(selectCoins);
    contextMenu->addAction(deselectCoins);
    contextMenu->addSeparator();
    contextMenu->addAction(selectAllCoins);
    contextMenu->addAction(deselectAllCoins);
    contextMenu->addSeparator();
    contextMenu->addAction(copyAddressAction);
    contextMenu->addAction(copyLabelAction);
    contextMenu->addAction(copyAmountAction);
    contextMenu->addAction(copyTransactionAction);
    contextMenu->addSeparator();
    contextMenu->addAction(lockAction);
    contextMenu->addAction(unlockAction);
    contextMenu->addSeparator();
    contextMenu->addAction(expandAll);
    contextMenu->addAction(collapseAll);

    layout->addWidget(tree);
    layout->addWidget(table);
    layout->addWidget(treeBox);

    // By default hide the tree view
    {
        QSettings settings;
        if (settings.contains("showTreeMode")) {
            const auto b = settings.value("showTreeMode").toBool();
            listRb->setChecked(!b);
            treeRb->setChecked(b);
            showTree(b);
        } else {
            listRb->setChecked(true);
            treeRb->setChecked(false);
            showTree(false);
            settings.setValue("showTreeMode", false);
        }
    }

    connect(table, &QTableWidget::customContextMenuRequested, this, &BlocknetCoinControl::showContextMenu);
    connect(tree, &QTableWidget::customContextMenuRequested, this, &BlocknetCoinControl::showContextMenu);
    connect(table->horizontalHeader(), &QHeaderView::sortIndicatorChanged, this, [this](int column, Qt::SortOrder order) {
        QSettings settings;
        // ignore sorting on columns less than or equal to pad2
        if (column <= COLUMN_PADDING2 || column == COLUMN_PADDING3 || column == COLUMN_PADDING4
            || column == COLUMN_PADDING5 || column == COLUMN_PADDING6) {
            table->horizontalHeader()->setSortIndicator(settings.value("nCoinControlSortColumn").toInt(),
                    static_cast<Qt::SortOrder>(settings.value("nCoinControlSortOrder").toInt()));
            return;
        }
        settings.setValue("nCoinControlSortOrder", static_cast<int>(order));
        settings.setValue("nCoinControlSortColumn", column);
    });
    connect(tree->header(), &QHeaderView::sortIndicatorChanged, this, [this](int column, Qt::SortOrder order) {
        QSettings settings;
        // ignore sorting on columns less than or equal to pad2
        if (column <= COLUMN_PADDING2 || column == COLUMN_PADDING3 || column == COLUMN_PADDING4
            || column == COLUMN_PADDING5 || column == COLUMN_PADDING6) {
            tree->header()->setSortIndicator(settings.value("nCoinControlTreeSortColumn").toInt(),
                    static_cast<Qt::SortOrder>(settings.value("nCoinControlTreeSortOrder").toInt()));
            return;
        }
        settings.setValue("nCoinControlTreeSortOrder", static_cast<int>(order));
        settings.setValue("nCoinControlTreeSortColumn", column);
    });

    // Tree mode
    connect(group, static_cast<void(QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked), this, [this](int button) {
        const auto b = treeMode();
        QSettings settings;
        settings.setValue("showTreeMode", b);
        showTree(b);
    });

    connect(selectCoins, &QAction::triggered, this, [this]() {
        if (treeMode()) {
            auto items = tree->selectedItems();
            if (!items.empty()) {
                unwatch();
                updateTableUtxos(updateTreeCheckStates(items, Qt::Checked));
                watch();
                Q_EMIT tableUpdated();
            }
            return;
        }
        // Update the list
        auto *select = table->selectionModel();
        if (select->hasSelection()) {
            unwatch();
            auto idxs = select->selectedRows(COLUMN_CHECKBOX);
            updateTreeUtxos(updateTableCheckStates(idxs, Qt::Checked));
            watch();
            Q_EMIT tableUpdated();
        }
    });
    connect(deselectCoins, &QAction::triggered, this, [this]() {
        if (treeMode()) {
            auto items = tree->selectedItems();
            if (!items.empty()) {
                unwatch();
                updateTableUtxos(updateTreeCheckStates(items, Qt::Unchecked));
                watch();
                Q_EMIT tableUpdated();
            }
            return;
        }
        // Update the list
        auto *select = table->selectionModel();
        if (select->hasSelection()) {
            unwatch();
            auto idxs = select->selectedRows(COLUMN_CHECKBOX);
            updateTreeUtxos(updateTableCheckStates(idxs, Qt::Unchecked));
            watch();
            Q_EMIT tableUpdated();
        }
    });

    connect(selectAllCoins, &QAction::triggered, this, [this]() {
        unwatch();
        updateTableUtxos(updateTreeCheckStates(allTreeItems(), Qt::Checked));
        watch();
        Q_EMIT tableUpdated();
    });
    connect(deselectAllCoins, &QAction::triggered, this, [this]() {
        unwatch();
        updateTableUtxos(updateTreeCheckStates(allTreeItems(), Qt::Unchecked));
        watch();
        Q_EMIT tableUpdated();
    });

    connect(copyAmountAction, &QAction::triggered, this, [this]() {
        UTXO *utxo = nullptr;
        if (treeMode()) {
            if (contextItemTr && utxoForHash(getTransactionHash(contextItemTr), getVOut(contextItemTr), utxo) && utxo != nullptr && utxo->isValid())
                setClipboard(utxo->amount);
        } else if (contextItem && utxoForHash(getTransactionHash(contextItem), getVOut(contextItem), utxo) && utxo != nullptr && utxo->isValid())
            setClipboard(utxo->amount);
    });
    connect(copyLabelAction, &QAction::triggered, this, [this]() {
        UTXO *utxo = nullptr;
        if (treeMode()) {
            if (contextItemTr && utxoForHash(getTransactionHash(contextItemTr), getVOut(contextItemTr), utxo) && utxo != nullptr && utxo->isValid())
                setClipboard(utxo->label);
        } else if (contextItem && utxoForHash(getTransactionHash(contextItem), getVOut(contextItem), utxo) && utxo != nullptr && utxo->isValid())
            setClipboard(utxo->label);
    });
    connect(copyAddressAction, &QAction::triggered, this, [this]() {
        UTXO *utxo = nullptr;
        if (treeMode()) {
            if (contextItemTr && utxoForHash(getTransactionHash(contextItemTr), getVOut(contextItemTr), utxo) && utxo != nullptr && utxo->isValid())
                setClipboard(utxo->address);
        } else if (contextItem && utxoForHash(getTransactionHash(contextItem), getVOut(contextItem), utxo) && utxo != nullptr && utxo->isValid())
            setClipboard(utxo->address);
    });
    connect(copyTransactionAction, &QAction::triggered, this, [this]() {
        UTXO *utxo = nullptr;
        if (treeMode()) {
            if (contextItemTr && utxoForHash(getTransactionHash(contextItemTr), getVOut(contextItemTr), utxo) && utxo != nullptr && utxo->isValid())
                setClipboard(utxo->transaction);
        } else if (contextItem && utxoForHash(getTransactionHash(contextItem), getVOut(contextItem), utxo) && utxo != nullptr && utxo->isValid())
            setClipboard(utxo->transaction);
    });

    connect(lockAction, &QAction::triggered, this, [this]() {
        const bool locked{true};
        if (treeMode()) {
            auto items = tree->selectedItems();
            if (!items.empty()) {
                unwatch();
                updateTableUtxos(updateTreeCheckStates(items, Qt::Unchecked, &locked));
                watch();
                Q_EMIT tableUpdated();
            }
            return;
        }
        // Update the list
        auto *select = table->selectionModel();
        if (select->hasSelection()) {
            unwatch();
            auto idxs = select->selectedRows(COLUMN_CHECKBOX);
            updateTreeUtxos(updateTableCheckStates(idxs, Qt::Unchecked, &locked));
            watch();
            Q_EMIT tableUpdated();
        }
    });
    connect(unlockAction, &QAction::triggered, this, [this]() {
        const bool locked{false};
        if (treeMode()) {
            auto items = tree->selectedItems();
            if (!items.empty()) {
                unwatch();
                updateTableUtxos(updateTreeCheckStates(items, Qt::Unchecked, &locked));
                watch();
                Q_EMIT tableUpdated();
            }
            return;
        }
        // Update the list
        auto *select = table->selectionModel();
        if (select->hasSelection()) {
            unwatch();
            auto idxs = select->selectedRows(COLUMN_CHECKBOX);
            updateTreeUtxos(updateTableCheckStates(idxs, Qt::Unchecked, &locked));
            watch();
            Q_EMIT tableUpdated();
        }
    });

    connect(expandAll, &QAction::triggered, this, [this]() {
        tree->expandAll();
    });
    connect(collapseAll, &QAction::triggered, this, [this]() {
        tree->collapseAll();
    });

    table->installEventFilter(this);
    tree->installEventFilter(this);
}

void BlocknetCoinControl::setData(ModelPtr dataModel) {
    this->dataModel = dataModel;

    unwatch();
    table->clearContents();
    table->setRowCount(dataModel->data.count());
    table->setSortingEnabled(false);
    tree->clear();
    tree->setSortingEnabled(false);

    std::map<QString, BlocknetCoinControl::TreeWidgetItem*> topLevelItems;

    for (int i = 0; i < dataModel->data.count(); ++i) {
        auto *d = dataModel->data[i];

        // Tree top level item
        BlocknetCoinControl::TreeWidgetItem *topLevelItemTr = nullptr;
        if (topLevelItems.count(d->address))
            topLevelItemTr = topLevelItems[d->address];
        else {
            topLevelItemTr = new BlocknetCoinControl::TreeWidgetItem;
            topLevelItemTr->setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsAutoTristate);
            topLevelItemTr->setText(COLUMN_LABEL, d->label);
            topLevelItemTr->setText(COLUMN_ADDRESS, d->address);
            topLevelItemTr->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);
            topLevelItems[d->address] = topLevelItemTr;
            tree->addTopLevelItem(topLevelItemTr);
        }

        // Current tree item
        auto *treeItem = new BlocknetCoinControl::TreeWidgetItem(topLevelItemTr);

        // checkbox
        auto *cbItem = new QTableWidgetItem;
        if (d->locked) {
            cbItem->setIcon(QIcon(":/redesign/lock_closed_white"));
            treeItem->setIcon(COLUMN_CHECKBOX, QIcon(":/redesign/lock_closed_white"));
        } else {
            cbItem->setCheckState(d->checked ? Qt::Checked : Qt::Unchecked);
            treeItem->setCheckState(COLUMN_CHECKBOX, d->checked ? Qt::Checked : Qt::Unchecked);
        }
        table->setItem(i, COLUMN_CHECKBOX, cbItem);

        // amount
        auto *amountItem = new BlocknetCoinControl::NumberItem;
        amountItem->setData(Qt::DisplayRole, d->amount);
        treeItem->setData(COLUMN_AMOUNT, Qt::DisplayRole, d->amount);
        treeItem->setData(COLUMN_AMOUNT, Qt::UserRole, static_cast<long long>(d->camount));
        table->setItem(i, COLUMN_AMOUNT, amountItem);
        // Add the amount to the associated top level item's total
        topLevelItemTr->camount += d->camount;
        topLevelItemTr->setData(COLUMN_AMOUNT, Qt::DisplayRole, BitcoinUnits::format(walletModel->getOptionsModel()->getDisplayUnit(), topLevelItemTr->camount));
        topLevelItemTr->setData(COLUMN_AMOUNT, Qt::UserRole, static_cast<long long>(topLevelItemTr->camount));

        // label
        auto *labelItem = new LabelItem;
        labelItem->setText(d->label);
        table->setItem(i, COLUMN_LABEL, labelItem);
        treeItem->setText(COLUMN_LABEL, d->label);

        // address
        auto *addressItem = new QTableWidgetItem;
        addressItem->setText(d->address);
        table->setItem(i, COLUMN_ADDRESS, addressItem);
        treeItem->setText(COLUMN_ADDRESS, d->address);

        // date
        auto *dateItem = new QTableWidgetItem;
        auto localDate = d->date.toLocalTime();
        dateItem->setData(Qt::DisplayRole, d->date);
        treeItem->setData(COLUMN_DATE, Qt::DisplayRole, d->date);
        treeItem->setData(COLUMN_DATE, Qt::UserRole, d->date);
        table->setItem(i, COLUMN_DATE, dateItem);

        // confirmations
        auto *confItem = new BlocknetCoinControl::NumberItem;
        confItem->setData(Qt::DisplayRole, QString::number(d->confirmations));
        treeItem->setData(COLUMN_CONFIRMATIONS, Qt::DisplayRole, static_cast<int>(d->confirmations));
        treeItem->setData(COLUMN_CONFIRMATIONS, Qt::UserRole, static_cast<int>(d->confirmations));
        table->setItem(i, COLUMN_CONFIRMATIONS, confItem);

        // txhash
        auto *txhashItem = new QTableWidgetItem;
        txhashItem->setData(Qt::DisplayRole, d->transaction);
        treeItem->setData(COLUMN_TXHASH, Qt::DisplayRole, d->transaction);
        table->setItem(i, COLUMN_TXHASH, txhashItem);

        // tx vout
        auto *txvoutItem = new QTableWidgetItem;
        txvoutItem->setData(Qt::DisplayRole, d->vout);
        treeItem->setData(COLUMN_TXVOUT, Qt::DisplayRole, d->vout);
        table->setItem(i, COLUMN_TXVOUT, txvoutItem);
    }

    table->setSortingEnabled(true);
    tree->setSortingEnabled(true);

    // Restore sorting preferences
    QSettings s;
    if (s.contains("nCoinControlSortColumn") && s.contains("nCoinControlSortOrder")) {
        table->horizontalHeader()->setSortIndicator(s.value("nCoinControlSortColumn").toInt(),
                                                    static_cast<Qt::SortOrder>(s.value("nCoinControlSortOrder").toInt()));
    } else {
        table->horizontalHeader()->setSortIndicator(COLUMN_LABEL, Qt::SortOrder::AscendingOrder);
    }
    if (s.contains("nCoinControlTreeSortOrder") && s.contains("nCoinControlTreeSortOrder")) {
        tree->header()->setSortIndicator(s.value("nCoinControlTreeSortOrder").toInt(),
                                         static_cast<Qt::SortOrder>(s.value("nCoinControlTreeSortOrder").toInt()));
    } else {
        tree->header()->setSortIndicator(COLUMN_LABEL, Qt::SortOrder::AscendingOrder);
    }

    watch();
}

BlocknetCoinControl::ModelPtr BlocknetCoinControl::getData() {
    return dataModel;
}

void BlocknetCoinControl::sizeTo(const int minimumHeight, const int maximumHeight) {
    int h = dataModel ? dataModel->data.count() * 25 : minimumHeight;
    if (h > maximumHeight)
        h = maximumHeight;
    table->setFixedHeight(h);
}

void BlocknetCoinControl::showContextMenu(QPoint pt) {
    if (treeMode()) {
        auto *select = tree->selectionModel();
        selectCoins->setEnabled(select->hasSelection());
        deselectCoins->setEnabled(select->hasSelection());
        selectAllCoins->setEnabled(select->hasSelection());
        deselectAllCoins->setEnabled(select->hasSelection());
        lockAction->setEnabled(select->hasSelection());
        unlockAction->setEnabled(select->hasSelection());
        expandAll->setEnabled(true);
        collapseAll->setEnabled(true);
        auto *item = tree->itemAt(pt);
        if (!item) {
            contextItemTr = nullptr;
            return;
        }
        copyAmountAction->setEnabled(item->childCount() <= 0);
        copyLabelAction->setEnabled(item->childCount() <= 0);
        copyAddressAction->setEnabled(item->childCount() <= 0);
        copyTransactionAction->setEnabled(item->childCount() <= 0);
        contextItemTr = item;
        contextItem = nullptr;
    } else {
        auto *select = table->selectionModel();
        selectCoins->setEnabled(select->hasSelection());
        deselectCoins->setEnabled(select->hasSelection());
        selectAllCoins->setEnabled(select->hasSelection());
        deselectAllCoins->setEnabled(select->hasSelection());
        lockAction->setEnabled(select->hasSelection());
        unlockAction->setEnabled(select->hasSelection());
        expandAll->setEnabled(false);
        collapseAll->setEnabled(false);
        auto *item = table->itemAt(pt);
        if (!item) {
            contextItem = nullptr;
            return;
        }
        copyAmountAction->setEnabled(true);
        copyLabelAction->setEnabled(true);
        copyAddressAction->setEnabled(true);
        copyTransactionAction->setEnabled(true);
        contextItem = item;
        contextItemTr = nullptr;
    }
    contextMenu->exec(QCursor::pos());
}

void BlocknetCoinControl::setClipboard(const QString &str) {
    QApplication::clipboard()->setText(str, QClipboard::Clipboard);
}

/**
 * @brief  Returns the string representation of the priority.
 * @param  dPriority
 * @return
 */
QString BlocknetCoinControl::getPriorityLabel(double dPriority) {
    double dPriorityMedium = dataModel->mempoolPriority;

    if (dPriorityMedium <= 0)
        dPriorityMedium = dataModel->freeThreshold;

    if (dPriority / 1000000 > dPriorityMedium)
        return tr("highest");
    else if (dPriority / 100000 > dPriorityMedium)
        return tr("higher");
    else if (dPriority / 10000 > dPriorityMedium)
        return tr("high");
    else if (dPriority / 1000 > dPriorityMedium)
        return tr("medium-high");
    else if (dPriority > dPriorityMedium)
        return tr("medium");
    else if (dPriority * 10 > dPriorityMedium)
        return tr("low-medium");
    else if (dPriority * 100 > dPriorityMedium)
        return tr("low");
    else if (dPriority * 1000 > dPriorityMedium)
        return tr("lower");
    else
        return tr("lowest");
}

void BlocknetCoinControl::unwatch() {
    table->blockSignals(true);
    tree->blockSignals(true);
    table->setEnabled(false);
    tree->setEnabled(false);
    disconnect(table, &QTableWidget::itemChanged, this, &BlocknetCoinControl::onItemChanged);
    disconnect(tree, &QTreeWidget::itemChanged, this, &BlocknetCoinControl::onTreeItemChanged);
}

void BlocknetCoinControl::watch() {
    table->blockSignals(false);
    tree->blockSignals(false);
    table->setEnabled(true);
    tree->setEnabled(true);
    connect(table, &QTableWidget::itemChanged, this, &BlocknetCoinControl::onItemChanged);
    connect(tree, &QTreeWidget::itemChanged, this, &BlocknetCoinControl::onTreeItemChanged);
}

bool BlocknetCoinControl::utxoForHash(const QString transaction, const uint vout, UTXO *&utxo) {
    for (auto *item : dataModel->data) {
        if (transaction == item->transaction && vout == item->vout) {
            utxo = item;
            return true;
        }
    }
    return false;
}

QString BlocknetCoinControl::getTransactionHash(QTableWidgetItem *item) {
    return table->item(item->row(), COLUMN_TXHASH)->data(Qt::DisplayRole).toString();
}

uint BlocknetCoinControl::getVOut(QTableWidgetItem *item) {
    return table->item(item->row(), COLUMN_TXVOUT)->data(Qt::DisplayRole).toUInt();
}

QString BlocknetCoinControl::getTransactionHash(QTreeWidgetItem *item) {
    return item->data(COLUMN_TXHASH, Qt::DisplayRole).toString();
}

uint BlocknetCoinControl::getVOut(QTreeWidgetItem *item) {
    return item->data(COLUMN_TXVOUT, Qt::DisplayRole).toUInt();
}

bool BlocknetCoinControl::treeMode() {
    return treeRb->isChecked();
}

void BlocknetCoinControl::showTree(bool yes) {
    if (yes) {
        table->setDisabled(true);
        table->hide();
        tree->setDisabled(false);
        tree->show();
        tree->setFocus(Qt::FocusReason::MouseFocusReason);
    } else {
        tree->setDisabled(true);
        tree->hide();
        table->setDisabled(false);
        table->show();
        table->setFocus(Qt::FocusReason::MouseFocusReason);
    }
}

BlocknetCoinControl::UTXO* BlocknetCoinControl::getTableUtxo(QTableWidgetItem *item, int row) {
    UTXO *utxo = nullptr;
    if (utxoForHash(getTransactionHash(item), getVOut(item), utxo)) {
        if (!utxo->isValid())
            utxo = nullptr;
    }
    return utxo;
}

BlocknetCoinControl::UTXO* BlocknetCoinControl::getTreeUtxo(QTreeWidgetItem *item) {
    UTXO *utxo = nullptr;
    if (utxoForHash(getTransactionHash(item), getVOut(item), utxo)) {
        if (!utxo->isValid())
            utxo = nullptr;
    }
    return utxo;
}

void BlocknetCoinControl::onItemChanged(QTableWidgetItem *item) {
    unwatch();
    auto idx = table->itemIndex(item);
    UTXO *utxo = getTableUtxo(item, idx.row());
    if (utxo && utxo->locked)
        item->setCheckState(Qt::Unchecked);
    else
        updateTreeUtxos(updateTableCheckStates({idx}, item->checkState()));
    watch();
    Q_EMIT tableUpdated();
}

void BlocknetCoinControl::onTreeItemChanged(QTreeWidgetItem *item) {
    if (item->checkState(COLUMN_CHECKBOX) == Qt::PartiallyChecked) // ignore indeterminate state changes
        return;
    unwatch();
    UTXO *utxo = getTreeUtxo(item);
    if (utxo && utxo->locked)
        item->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);
    else
        updateTableUtxos(updateTreeCheckStates({item}, item->checkState(COLUMN_CHECKBOX)));
    watch();
    Q_EMIT tableUpdated();
}

bool BlocknetCoinControl::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::KeyPress && (obj == table || obj == tree)) {
        auto *keyEvent = dynamic_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Space) {
            if (obj == table && table->selectionModel()->hasSelection()) {
                auto *select = table->selectionModel();
                if (select->hasSelection()) {
                    unwatch();
                    QMap<std::string, UTXO*> utxos;
                    auto idxs = select->selectedRows(COLUMN_CHECKBOX);
                    for (auto & idx : idxs) {
                        auto *item = table->item(idx.row(), idx.column());
                        if (item) {
                            UTXO *utxo = getTableUtxo(item, idx.row());
                            if (utxo && !utxo->locked)
                                utxos[utxo->toString()] = utxo;
                        }
                    }
                    // First check if they're all selected to determine
                    // whether to deselect. Only deselect if all utxos
                    // are selected.
                    bool shouldDeselect{true};
                    for (auto & item : utxos) {
                        if (!item->checked) {
                            shouldDeselect = false;
                            break;
                        }
                    }
                    for (auto & item : utxos)
                        item->checked = !shouldDeselect;
                    // Update list
                    for (auto & idx : idxs) {
                        auto *item = table->item(idx.row(), idx.column());
                        if (item)
                            item->setCheckState(!shouldDeselect ? Qt::Checked : Qt::Unchecked);
                    }
                    // Update tree
                    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
                        auto *topLevelItem = tree->topLevelItem(i);
                        for (int j = 0; j < topLevelItem->childCount(); ++j) {
                            auto *item = topLevelItem->child(j);
                            auto *utxo = getTreeUtxo(item);
                            if (utxo && utxos.count(utxo->toString()))
                                item->setCheckState(COLUMN_CHECKBOX, utxos[utxo->toString()]->checked ? Qt::Checked : Qt::Unchecked);
                        }
                    }
                    watch();
                    table->setFocus(Qt::FocusReason::MouseFocusReason);
                    Q_EMIT tableUpdated();
                }
            } else if (obj == tree && !tree->selectedItems().isEmpty()) {
                auto items = tree->selectedItems();
                if (!items.empty()) {
                    unwatch();
                    QMap<std::string, UTXO*> utxos;
                    QList<QTreeWidgetItem*> qitems;
                    // Update tree
                    for (auto & item : items) {
                        if (item->childCount() > 0) {
                            for (int i = 0; i < item->childCount(); ++i)
                                qitems.push_back(item->child(i));
                        } else
                            qitems.push_back(item);
                    }
                    for (auto & qitem : qitems) {
                        UTXO *utxo = getTreeUtxo(qitem);
                        if (utxo && !utxo->locked)
                            utxos[utxo->toString()] = utxo;
                    }
                    // First check if they're all selected to determine
                    // whether to deselect. Only deselect if all utxos
                    // are selected.
                    bool shouldDeselect{true};
                    for (auto & item : utxos) {
                        if (!item->checked) {
                            shouldDeselect = false;
                            break;
                        }
                    }
                    for (auto & item : utxos)
                        item->checked = !shouldDeselect;
                    // Update tree
                    for (auto & qitem : qitems)
                        qitem->setCheckState(COLUMN_CHECKBOX, !shouldDeselect ? Qt::Checked : Qt::Unchecked);
                    // Update list
                    for (int row = 0; row < table->rowCount(); row++) {
                        auto *item = table->item(row, COLUMN_TXHASH);
                        auto *utxo = getTableUtxo(item, row);
                        if (utxo && utxos.count(utxo->toString())) {
                            item = table->item(row, COLUMN_CHECKBOX);
                            item->setCheckState(!shouldDeselect ? Qt::Checked : Qt::Unchecked);
                        }
                    }
                    watch();
                    tree->setFocus(Qt::FocusReason::MouseFocusReason);
                    Q_EMIT tableUpdated();
                }
            }
            return true; // done
        }
    }
    return QObject::eventFilter(obj, event);
}

void BlocknetCoinControl::updateTableUtxos(const QMap<std::string, UTXO*> & utxos) {
    for (int row = 0; row < table->rowCount(); row++) {
        auto *item = table->item(row, COLUMN_TXHASH);
        auto *utxo = getTableUtxo(item, row);
        if (utxo && utxos.count(utxo->toString())) {
            auto *putxo = utxos[utxo->toString()];
            auto *cbItem = new QTableWidgetItem;
            if (putxo->locked) {
                cbItem->setIcon(QIcon(":/redesign/lock_closed_white"));
                cbItem->setFlags(cbItem->flags() ^ Qt::ItemIsEditable);
            } else {
                cbItem->setCheckState(putxo->checked ? Qt::Checked : Qt::Unchecked);
                cbItem->setFlags(cbItem->flags() | Qt::ItemIsEditable);
            }
            table->setItem(row, COLUMN_CHECKBOX, cbItem);
        }
    }
}

void BlocknetCoinControl::updateTreeUtxos(const QMap<std::string, UTXO*> & utxos) {
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        auto *topLevelItem = tree->topLevelItem(i);
        for (int j = 0; j < topLevelItem->childCount(); ++j) {
            auto *item = topLevelItem->child(j);
            auto *utxo = getTreeUtxo(item);
            if (utxo && utxos.count(utxo->toString())) {
                auto *putxo = utxos[utxo->toString()];
                if (putxo->locked) {
                    item->setIcon(COLUMN_CHECKBOX, QIcon(":/redesign/lock_closed_white"));
                    item->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);
                    item->setFlags(item->flags() ^ Qt::ItemIsEditable);
                } else {
                    item->setIcon(COLUMN_CHECKBOX, QIcon());
                    item->setCheckState(COLUMN_CHECKBOX, putxo->checked ? Qt::Checked : Qt::Unchecked);
                    item->setFlags(item->flags() | Qt::ItemIsEditable);
                }
            }
        }
    }
}

QMap<std::string, BlocknetCoinControl::UTXO*> BlocknetCoinControl::updateTableCheckStates(const QList<QModelIndex> & idxs, Qt::CheckState checkState, const bool *lockState) {
    QMap<std::string, UTXO*> utxos;
    for (auto & idx : idxs) {
        auto *item = table->item(idx.row(), idx.column());
        if (item) {
            UTXO *utxo = getTableUtxo(item, idx.row());
            if (lockState == nullptr && utxo && !utxo->locked && utxo->isValid()) {
                utxo->checked = checkState == Qt::Checked;
                item->setCheckState(checkState);
                utxos[utxo->toString()] = utxo;
            } else if (lockState != nullptr && utxo && utxo->isValid()) { // if lock state
                utxo->locked = *lockState;
                utxo->unlocked = !utxo->locked;
                utxo->checked = false;
                auto *cbItem = new QTableWidgetItem;
                if (utxo->locked) {
                    cbItem->setIcon(QIcon(":/redesign/lock_closed_white"));
                    cbItem->setFlags(cbItem->flags() ^ Qt::ItemIsEditable);
                    walletModel->wallet().lockCoin({uint256S(utxo->transaction.toStdString()), utxo->vout});
                } else {
                    cbItem->setCheckState(Qt::Unchecked);
                    cbItem->setFlags(cbItem->flags() | Qt::ItemIsEditable);
                    walletModel->wallet().unlockCoin({uint256S(utxo->transaction.toStdString()), utxo->vout});
                }
                table->setItem(idx.row(), COLUMN_CHECKBOX, cbItem);
                utxos[utxo->toString()] = utxo;
            }
        }
    }
    return utxos;
}

QMap<std::string, BlocknetCoinControl::UTXO*> BlocknetCoinControl::updateTreeCheckStates(const QList<QTreeWidgetItem*> & items, Qt::CheckState checkState, const bool *lockState) {
    QMap<std::string, UTXO*> utxos;
    QList<QTreeWidgetItem*> qitems;
    for (auto & item : items) {
        if (item->childCount() > 0) {
            for (int i = 0; i < item->childCount(); ++i)
                qitems.push_back(item->child(i));
        } else
            qitems.push_back(item);
    }
    for (auto & qitem : qitems) {
        UTXO *utxo = getTreeUtxo(qitem);
        if (lockState == nullptr && utxo && !utxo->locked && utxo->isValid()) { // non-lock state
            utxo->checked = checkState == Qt::Checked;
            qitem->setCheckState(COLUMN_CHECKBOX, checkState);
            utxos[utxo->toString()] = utxo;
        } else if (lockState != nullptr && utxo && utxo->isValid()) { // if lock state
            utxo->locked = *lockState;
            utxo->unlocked = !utxo->locked;
            utxo->checked = false;
            if (utxo->locked) {
                qitem->setIcon(COLUMN_CHECKBOX, QIcon(":/redesign/lock_closed_white"));
                qitem->setFlags(qitem->flags() ^ Qt::ItemIsEditable);
                walletModel->wallet().lockCoin({uint256S(utxo->transaction.toStdString()), utxo->vout});
            } else {
                qitem->setIcon(COLUMN_CHECKBOX, QIcon());
                qitem->setFlags(qitem->flags() | Qt::ItemIsEditable);
                walletModel->wallet().unlockCoin({uint256S(utxo->transaction.toStdString()), utxo->vout});
            }
            qitem->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);
            utxos[utxo->toString()] = utxo;
        }
    }
    return utxos;
}

QList<QTreeWidgetItem*> BlocknetCoinControl::allTreeItems() {
    QList<QTreeWidgetItem*> r;
    for (int row = 0; row < tree->topLevelItemCount(); ++row) {
        auto *topLevelItem = tree->topLevelItem(row);
        if (!topLevelItem)
            continue;
        for (int childIdx = 0; childIdx < topLevelItem->childCount(); ++childIdx) {
            auto *item = topLevelItem->child(childIdx);
            if (!item)
                continue;
            r.push_back(item);
        }
    }
    return r;
}