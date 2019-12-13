// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetwalletrepair.h>

#include <qt/blocknetguiutil.h>
#include <qt/blocknethdiv.h>

#include <QApplication>
#include <QMessageBox>

// Repair parameters
const QString SALVAGEWALLET("-salvagewallet");
const QString RESCAN("-rescan");
const QString ZAPTXES1("-zapwallettxes=1");
const QString ZAPTXES2("-zapwallettxes=2");
const QString UPGRADEWALLET("-upgradewallet");
const QString REINDEX("-reindex");

BlocknetWalletRepair::BlocknetWalletRepair(QWidget *popup, int id, QFrame *parent) : BlocknetToolsPage(id, parent),
                                                                                     popupWidget(popup),
                                                                                     layout(new QVBoxLayout)
{
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(layout);
    layout->setContentsMargins(QMargins());
    layout->setSpacing(0);

    content = new QFrame;
    content->setContentsMargins(QMargins());
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    content->setObjectName("contentFrame");
    contentLayout = new QVBoxLayout;
    contentLayout->setContentsMargins(QMargins());
    contentLayout->setSizeConstraint(QLayout::SetMinimumSize);
    content->setLayout(contentLayout);
    
    scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setContentsMargins(QMargins());
    scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setWidget(content);

    titleLbl = new QLabel(tr("Wallet Repair"));
    titleLbl->setObjectName("h2");

    descriptionTxt = new QLabel(tr("The options below will restart the wallet with command-line options to repair the "
                                   "wallet and fix issues with corrupt blockchain files or missing/obsolete transactions."));
    descriptionTxt->setObjectName("notes");
    descriptionTxt->setWordWrap(true);

    auto *div1 = new BlocknetHDiv;

    walletFrame = new QFrame;
    walletFrame->setContentsMargins(QMargins());
    walletFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    walletLayout = new QHBoxLayout;
    walletLayout->setContentsMargins(QMargins());
    walletFrame->setLayout(walletLayout);

    salvageFrame = new QFrame;
    salvageFrame->setContentsMargins(QMargins());
    salvageFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    salvageLayout = new QVBoxLayout;
    salvageLayout->setContentsMargins(QMargins());
    salvageFrame->setLayout(salvageLayout);

    salvageTitleLbl = new QLabel(tr("Salvage Wallet"));
    salvageTitleLbl->setObjectName("title");

    salvageDescLbl = new QLabel(tr("Attempt to recover private keys from a corrupt wallet.dat"));
    salvageDescLbl->setObjectName("description");
    salvageDescLbl->setWordWrap(true);

    salvageLayout->addWidget(salvageTitleLbl, 0, Qt::AlignTop);
    salvageLayout->addWidget(salvageDescLbl, 0, Qt::AlignTop);

    salvageWalletBtn = new BlocknetFormBtn;
    salvageWalletBtn->setText(tr("Salvage Wallet"));

    walletLayout->addWidget(salvageFrame);
    walletLayout->addWidget(salvageWalletBtn);

    auto *salvageDiv = new BlocknetHDiv;

    rescanFrame = new QFrame;
    rescanFrame->setContentsMargins(QMargins());
    rescanFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    rescanLayout = new QHBoxLayout;
    rescanLayout->setContentsMargins(QMargins());
    rescanFrame->setLayout(rescanLayout);

    blockchainFrame = new QFrame;
    blockchainFrame->setContentsMargins(QMargins());
    blockchainFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    blockchainLayout = new QVBoxLayout;
    blockchainLayout->setContentsMargins(QMargins());
    blockchainFrame->setLayout(blockchainLayout);

    rescanTitleLbl = new QLabel(tr("Rescan Blockchain Files"));
    rescanTitleLbl->setObjectName("title");

    rescanDescLbl = new QLabel(tr("Rescan the blockchain for missing wallet transactions."));
    rescanDescLbl->setObjectName("description");
    rescanDescLbl->setWordWrap(true);

    blockchainLayout->addWidget(rescanTitleLbl, 0, Qt::AlignTop);
    blockchainLayout->addWidget(rescanDescLbl, 0, Qt::AlignTop);

    rescanBlockchainBtn = new BlocknetFormBtn;
    rescanBlockchainBtn->setText(tr("Rescan"));

    rescanLayout->addWidget(blockchainFrame);
    rescanLayout->addWidget(rescanBlockchainBtn);

    auto *rescanDiv = new BlocknetHDiv;

    transaction1Frame = new QFrame;
    transaction1Frame->setContentsMargins(QMargins());
    transaction1Frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    transaction1Layout = new QHBoxLayout;
    transaction1Layout->setContentsMargins(QMargins());
    transaction1Frame->setLayout(transaction1Layout);

    recoverFrame = new QFrame;
    recoverFrame->setContentsMargins(QMargins());
    recoverFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    recoverLayout = new QVBoxLayout;
    recoverLayout->setContentsMargins(QMargins());
    recoverFrame->setLayout(recoverLayout);

    transaction1TitleLbl = new QLabel(tr("Recover Transactions (Keep metadata)"));
    transaction1TitleLbl->setObjectName("title");

    transaction1DescLbl = new QLabel(tr("Equivalent to -zapwallettxes=1. Recover transactions from blockchain and keep metadata, e.g. payment request information."));
    transaction1DescLbl->setObjectName("description");
    transaction1DescLbl->setWordWrap(true);

    recoverLayout->addWidget(transaction1TitleLbl, 0, Qt::AlignTop);
    recoverLayout->addWidget(transaction1DescLbl, 0, Qt::AlignTop);

    transaction1Btn = new BlocknetFormBtn;
    transaction1Btn->setText(tr("Recover"));

    transaction1Layout->addWidget(recoverFrame);
    transaction1Layout->addWidget(transaction1Btn);

    auto *transaction1Div = new BlocknetHDiv;

    transaction2Frame = new QFrame;
    transaction2Frame->setContentsMargins(QMargins());
    transaction2Frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    transaction2Layout = new QHBoxLayout;
    transaction2Layout->setContentsMargins(QMargins());
    transaction2Frame->setLayout(transaction2Layout);

    recover2Frame = new QFrame;
    recover2Frame->setContentsMargins(QMargins());
    recover2Frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    recover2Layout = new QVBoxLayout;
    recover2Layout->setContentsMargins(QMargins());
    recover2Frame->setLayout(recover2Layout);

    transaction2TitleLbl = new QLabel(tr("Recover Transactions (Drop metadata)"));
    transaction2TitleLbl->setObjectName("title");

    transaction2DescLbl = new QLabel(tr("Equivalent to -zapwallettxes=2. Recover transactions from blockchain and drop tx metadata."));
    transaction2DescLbl->setObjectName("description");
    transaction2DescLbl->setWordWrap(true);

    recover2Layout->addWidget(transaction2TitleLbl, 0, Qt::AlignTop);
    recover2Layout->addWidget(transaction2DescLbl, 0, Qt::AlignTop);

    transaction2Btn = new BlocknetFormBtn;
    transaction2Btn->setText(tr("Recover"));

    transaction2Layout->addWidget(recover2Frame);
    transaction2Layout->addWidget(transaction2Btn);

    auto *transaction2Div = new BlocknetHDiv;

    upgradeFrame = new QFrame;
    upgradeFrame->setContentsMargins(QMargins());
    upgradeFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    upgradeLayout = new QHBoxLayout;
    upgradeLayout->setContentsMargins(QMargins());
    upgradeFrame->setLayout(upgradeLayout);

    formatFrame = new QFrame;
    formatFrame->setContentsMargins(QMargins());
    formatFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    formatLayout = new QVBoxLayout;
    formatLayout->setContentsMargins(QMargins());
    formatFrame->setLayout(formatLayout);

    formatTitleLbl = new QLabel(tr("Upgrade Format"));
    formatTitleLbl->setObjectName("title");

    formatDescLbl = new QLabel(tr("Upgrade wallet to the latest format on startup. (This is not an update of the wallet itself)."));
    formatDescLbl->setObjectName("description");
    formatDescLbl->setWordWrap(true);

    formatLayout->addWidget(formatTitleLbl, 0, Qt::AlignTop);
    formatLayout->addWidget(formatDescLbl, 0, Qt::AlignTop);

    upgradeBtn = new BlocknetFormBtn;
    upgradeBtn->setText(tr("Upgrade Format"));

    upgradeLayout->addWidget(formatFrame);
    upgradeLayout->addWidget(upgradeBtn);

    auto *upgradeDiv = new BlocknetHDiv;

    rebuildFrame = new QFrame;
    rebuildFrame->setContentsMargins(QMargins());
    rebuildFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    rebuildLayout = new QHBoxLayout;
    rebuildLayout->setContentsMargins(QMargins());
    rebuildFrame->setLayout(rebuildLayout);

    indexFrame = new QFrame;
    indexFrame->setContentsMargins(QMargins());
    indexFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    indexLayout = new QVBoxLayout;
    indexLayout->setContentsMargins(QMargins());
    indexFrame->setLayout(indexLayout);

    indexTitleLbl = new QLabel(tr("Rebuild Index"));
    indexTitleLbl->setObjectName("title");

    indexDescLbl = new QLabel(tr("Rebuild blockchain index from current blk000.dat files."));
    indexDescLbl->setObjectName("description");
    indexDescLbl->setWordWrap(true);

    indexLayout->addWidget(indexTitleLbl, 0, Qt::AlignTop);
    indexLayout->addWidget(indexDescLbl, 0, Qt::AlignTop);

    rebuildBtn = new BlocknetFormBtn;
    rebuildBtn->setText(tr("Rebuild Index"));

    rebuildLayout->addWidget(indexFrame);
    rebuildLayout->addWidget(rebuildBtn);

    auto *rebuildDiv = new BlocknetHDiv;

    contentLayout->addWidget(titleLbl, 0, Qt::AlignTop);
    contentLayout->addSpacing(BGU::spi(5));
    contentLayout->addWidget(descriptionTxt, 0, Qt::AlignTop);
    contentLayout->addSpacing(BGU::spi(5));
    contentLayout->addWidget(div1);
    contentLayout->addSpacing(BGU::spi(5));
    contentLayout->addWidget(walletFrame);
    contentLayout->addSpacing(BGU::spi(5));
    contentLayout->addWidget(salvageDiv);
    contentLayout->addSpacing(BGU::spi(5));
    contentLayout->addWidget(rescanFrame);
    contentLayout->addSpacing(BGU::spi(5));
    contentLayout->addWidget(rescanDiv);
    contentLayout->addSpacing(BGU::spi(5));
    contentLayout->addWidget(transaction1Frame);
    contentLayout->addSpacing(BGU::spi(5));
    contentLayout->addWidget(transaction1Div);
    contentLayout->addSpacing(BGU::spi(5));
    contentLayout->addWidget(transaction2Frame);
    contentLayout->addSpacing(BGU::spi(5));
    contentLayout->addWidget(transaction2Div);
    contentLayout->addSpacing(BGU::spi(5));
    contentLayout->addWidget(upgradeFrame);
    contentLayout->addSpacing(BGU::spi(5));
    contentLayout->addWidget(upgradeDiv);
    contentLayout->addSpacing(BGU::spi(5));
    contentLayout->addWidget(rebuildFrame);
    contentLayout->addSpacing(BGU::spi(5));
    contentLayout->addWidget(rebuildDiv);
    contentLayout->addSpacing(BGU::spi(10));
    contentLayout->addStretch(0);

    layout->addWidget(scrollArea, 1);

    connect(salvageWalletBtn, &BlocknetFormBtn::clicked, this, &BlocknetWalletRepair::walletSalvage);
    connect(rescanBlockchainBtn, &BlocknetFormBtn::clicked, this, &BlocknetWalletRepair::walletRescan);
    connect(transaction1Btn, &BlocknetFormBtn::clicked, this, &BlocknetWalletRepair::walletZaptxes1);
    connect(transaction2Btn, &BlocknetFormBtn::clicked, this, &BlocknetWalletRepair::walletZaptxes2);
    connect(upgradeBtn, &BlocknetFormBtn::clicked, this, &BlocknetWalletRepair::walletUpgrade);
    connect(rebuildBtn, &BlocknetFormBtn::clicked, this, &BlocknetWalletRepair::walletReindex);
}

void BlocknetWalletRepair::setWalletModel(WalletModel *w) {
    if (!walletModel)
        return;
    walletModel = w;
}

/** Restart wallet with "-salvagewallet" */
void BlocknetWalletRepair::walletSalvage()
{
    if (displayConfirmationBox(tr("Restart Required"), tr("Are you sure you want to restart and salvage the wallet?\n\nThis can take several minutes.")))
        buildParameterlist(SALVAGEWALLET);
}

/** Restart wallet with "-rescan" */
void BlocknetWalletRepair::walletRescan()
{
    if (displayConfirmationBox(tr("Restart Required"), tr("Are you sure you want to rescan the blockchain for missing transactions?\n\nThis can take several minutes.")))
        buildParameterlist(RESCAN);
}

/** Restart wallet with "-zapwallettxes=1" */
void BlocknetWalletRepair::walletZaptxes1()
{
    if (displayConfirmationBox(tr("Restart Required"), tr("Are you sure you want to recover wallet transactions?\n\nThis can take several minutes.")))
        buildParameterlist(ZAPTXES1);
}

/** Restart wallet with "-zapwallettxes=2" */
void BlocknetWalletRepair::walletZaptxes2()
{
    if (displayConfirmationBox(tr("Restart Required"), tr("Are you sure you want to recover wallet transactions and discard metadata?\n\nThis can take several minutes.")))
        buildParameterlist(ZAPTXES2);
}

/** Restart wallet with "-upgradewallet" */
void BlocknetWalletRepair::walletUpgrade()
{
    if (displayConfirmationBox(tr("Restart Required"), tr("Are you sure you want to upgrade the wallet to HD?\n\nThis can take several minutes.")))
        buildParameterlist(UPGRADEWALLET);
}

/** Restart wallet with "-reindex" */
void BlocknetWalletRepair::walletReindex()
{
    if (displayConfirmationBox(tr("Restart Required"), tr("Are you sure you want to reindex the blockchain?\n\nThis can take several minutes.")))
        buildParameterlist(REINDEX);
}

/** Build command-line parameter list for restart */
void BlocknetWalletRepair::buildParameterlist(QString arg)
{
    // Get command-line arguments and remove the application name
    QStringList args = QApplication::arguments();
    args.removeFirst();

    // Remove existing repair-options
    args.removeAll(SALVAGEWALLET);
    args.removeAll(RESCAN);
    args.removeAll(ZAPTXES1);
    args.removeAll(ZAPTXES2);
    args.removeAll(UPGRADEWALLET);
    args.removeAll(REINDEX);

    // Append repair parameter to command line.
    args.append(arg);

    // Send command-line arguments to BitcoinGUI::handleRestart()
    Q_EMIT handleRestart(args);
}

/**
 * Display confirm message box.
 * @param title
 * @param msg
 */
bool BlocknetWalletRepair::displayConfirmationBox(const QString & title, const QString & msg) {
    auto retval = static_cast<QMessageBox::StandardButton>(QMessageBox::question(this, title, msg,
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel));
    return retval == QMessageBox::Yes;
}