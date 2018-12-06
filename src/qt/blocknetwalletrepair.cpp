// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetwalletrepair.h"

#include "blocknethdiv.h"

#include <QApplication>

// Repair parameters
const QString SALVAGEWALLET("-salvagewallet");
const QString RESCAN("-rescan");
const QString ZAPTXES1("-zapwallettxes=1");
const QString ZAPTXES2("-zapwallettxes=2");
const QString UPGRADEWALLET("-upgradewallet");
const QString REINDEX("-reindex");

BlocknetWalletRepair::BlocknetWalletRepair(QWidget *popup, int id, QFrame *parent) : BlocknetToolsPage(id, parent), popupWidget(popup), layout(new QVBoxLayout) {
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(layout);
    layout->setContentsMargins(QMargins());
    layout->setSpacing(0);

    content = new QFrame;
    content->setContentsMargins(QMargins());
    content->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum);
    content->setObjectName("contentFrame");
    contentLayout = new QVBoxLayout;
    contentLayout->setContentsMargins(QMargins());
    content->setLayout(contentLayout);
    
    scrollArea = new QScrollArea;
    scrollArea->setContentsMargins(QMargins());
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setWidget(content);

    titleLbl = new QLabel(tr("Wallet Repair"));
    titleLbl->setObjectName("h2");

    descriptionTxt = new QTextEdit(tr("The options below will restart the wallet with command-line options to repair the wallet and fix issues with corrupt blockchain files or missing/obsolete transactions."));
    descriptionTxt->setFixedHeight(45);
    descriptionTxt->setReadOnly(true);

    auto *div1 = new BlocknetHDiv;

    walletFrame = new QFrame;
    walletFrame->setContentsMargins(QMargins());
    walletFrame->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum);
    walletLayout = new QHBoxLayout;
    walletLayout->setContentsMargins(QMargins());
    walletFrame->setLayout(walletLayout);

    salvageFrame = new QFrame;
    salvageFrame->setContentsMargins(QMargins());
    salvageFrame->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum);
    salvageLayout = new QVBoxLayout;
    salvageLayout->setContentsMargins(QMargins());
    salvageFrame->setLayout(salvageLayout);

    salvageTitleLbl = new QLabel(tr("Salvage Wallet"));
    salvageTitleLbl->setObjectName("title");

    salvageDescLbl = new QLabel(tr("Attempt to recover private keys from a corrupt walle.dat."));
    salvageDescLbl->setObjectName("description");

    salvageLayout->addWidget(salvageTitleLbl);
    salvageLayout->addWidget(salvageDescLbl);

    salvageWalletBtn = new BlocknetFormBtn;
    salvageWalletBtn->setText(tr("Salvage Wallet"));

    walletLayout->addWidget(salvageFrame);
    walletLayout->addWidget(salvageWalletBtn);

    auto *salvageDiv = new BlocknetHDiv;

    rescanFrame = new QFrame;
    rescanFrame->setContentsMargins(QMargins());
    rescanFrame->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum);
    rescanLayout = new QHBoxLayout;
    rescanLayout->setContentsMargins(QMargins());
    rescanFrame->setLayout(rescanLayout);

    blockchainFrame = new QFrame;
    blockchainFrame->setContentsMargins(QMargins());
    blockchainFrame->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum);
    blockchainLayout = new QVBoxLayout;
    blockchainLayout->setContentsMargins(QMargins());
    blockchainFrame->setLayout(blockchainLayout);

    rescanTitleLbl = new QLabel(tr("Rescan Blockchain Files"));
    rescanTitleLbl->setObjectName("title");

    rescanDescLbl = new QLabel(tr("Rescan the blockchain for missing wallet transactions."));
    rescanDescLbl->setObjectName("description");

    blockchainLayout->addWidget(rescanTitleLbl);
    blockchainLayout->addWidget(rescanDescLbl);

    rescanBlockchainBtn = new BlocknetFormBtn;
    rescanBlockchainBtn->setText(tr("Rescan"));

    rescanLayout->addWidget(blockchainFrame);
    rescanLayout->addWidget(rescanBlockchainBtn);

    auto *rescanDiv = new BlocknetHDiv;

    transaction1Frame = new QFrame;
    transaction1Frame->setContentsMargins(QMargins());
    transaction1Frame->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum);
    transaction1Layout = new QHBoxLayout;
    transaction1Layout->setContentsMargins(QMargins());
    transaction1Frame->setLayout(transaction1Layout);

    recoverFrame = new QFrame;
    recoverFrame->setContentsMargins(QMargins());
    recoverFrame->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum);
    recoverLayout = new QVBoxLayout;
    recoverLayout->setContentsMargins(QMargins());
    recoverFrame->setLayout(recoverLayout);

    transaction1TitleLbl = new QLabel(tr("Recover Transactions 1"));
    transaction1TitleLbl->setObjectName("title");

    transaction1DescLbl = new QLabel(tr("Recover transactions from blockchain. (Keep meta-data)."));
    transaction1DescLbl->setObjectName("description");

    recoverLayout->addWidget(transaction1TitleLbl);
    recoverLayout->addWidget(transaction1DescLbl);

    transaction1Btn = new BlocknetFormBtn;
    transaction1Btn->setText(tr("Recover"));

    transaction1Layout->addWidget(recoverFrame);
    transaction1Layout->addWidget(transaction1Btn);

    auto *transaction1Div = new BlocknetHDiv;

    transaction2Frame = new QFrame;
    transaction2Frame->setContentsMargins(QMargins());
    transaction2Frame->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum);
    transaction2Layout = new QHBoxLayout;
    transaction2Layout->setContentsMargins(QMargins());
    transaction2Frame->setLayout(transaction2Layout);

    recover2Frame = new QFrame;
    recover2Frame->setContentsMargins(QMargins());
    recover2Frame->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum);
    recover2Layout = new QVBoxLayout;
    recover2Layout->setContentsMargins(QMargins());
    recover2Frame->setLayout(recover2Layout);

    transaction2TitleLbl = new QLabel(tr("Recover Transactions 2"));
    transaction2TitleLbl->setObjectName("title");

    transaction2DescLbl = new QLabel(tr("Recover transactions from blockchain. (Drop meta-data)."));
    transaction2DescLbl->setObjectName("description");

    recover2Layout->addWidget(transaction2TitleLbl);
    recover2Layout->addWidget(transaction2DescLbl);

    transaction2Btn = new BlocknetFormBtn;
    transaction2Btn->setText(tr("Recover"));

    transaction2Layout->addWidget(recover2Frame);
    transaction2Layout->addWidget(transaction2Btn);

    auto *transaction2Div = new BlocknetHDiv;

    upgradeFrame = new QFrame;
    upgradeFrame->setContentsMargins(QMargins());
    upgradeFrame->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum);
    upgradeLayout = new QHBoxLayout;
    upgradeLayout->setContentsMargins(QMargins());
    upgradeFrame->setLayout(upgradeLayout);

    formatFrame = new QFrame;
    formatFrame->setContentsMargins(QMargins());
    formatFrame->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum);
    formatLayout = new QVBoxLayout;
    formatLayout->setContentsMargins(QMargins());
    formatFrame->setLayout(formatLayout);

    formatTitleLbl = new QLabel(tr("Upgrade Format"));
    formatTitleLbl->setObjectName("title");

    formatDescLbl = new QLabel(tr("Upgrade wallet to the latest format on startup. (This is not an update of the wallet itself)."));
    formatDescLbl->setObjectName("description");

    formatLayout->addWidget(formatTitleLbl);
    formatLayout->addWidget(formatDescLbl);

    upgradeBtn = new BlocknetFormBtn;
    upgradeBtn->setText(tr("Upgrade Format"));

    upgradeLayout->addWidget(formatFrame);
    upgradeLayout->addWidget(upgradeBtn);

    auto *upgradeDiv = new BlocknetHDiv;

    rebuildFrame = new QFrame;
    rebuildFrame->setContentsMargins(QMargins());
    rebuildFrame->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum);
    rebuildLayout = new QHBoxLayout;
    rebuildLayout->setContentsMargins(QMargins());
    rebuildFrame->setLayout(rebuildLayout);

    indexFrame = new QFrame;
    indexFrame->setContentsMargins(QMargins());
    indexFrame->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum);
    indexLayout = new QVBoxLayout;
    indexLayout->setContentsMargins(QMargins());
    indexFrame->setLayout(indexLayout);

    indexTitleLbl = new QLabel(tr("Rebuild Index"));
    indexTitleLbl->setObjectName("title");

    indexDescLbl = new QLabel(tr("Rebuild blockchain index from current blk000.dat files."));
    indexDescLbl->setObjectName("description");

    indexLayout->addWidget(indexTitleLbl);
    indexLayout->addWidget(indexDescLbl);

    rebuildBtn = new BlocknetFormBtn;
    rebuildBtn->setText(tr("Rebuild Index"));

    rebuildLayout->addWidget(indexFrame);
    rebuildLayout->addWidget(rebuildBtn);

    auto *rebuildDiv = new BlocknetHDiv;

    contentLayout->addWidget(titleLbl, 0, Qt::AlignTop | Qt::AlignLeft);
    contentLayout->addSpacing(10);
    contentLayout->addWidget(descriptionTxt);
    contentLayout->addSpacing(10);
    contentLayout->addWidget(div1);
    contentLayout->addSpacing(10);
    contentLayout->addWidget(walletFrame);
    contentLayout->addSpacing(10);
    contentLayout->addWidget(salvageDiv);
    contentLayout->addSpacing(10);
    contentLayout->addWidget(rescanFrame);
    contentLayout->addSpacing(10);
    contentLayout->addWidget(rescanDiv);
    contentLayout->addSpacing(10);
    contentLayout->addWidget(transaction1Frame);
    contentLayout->addSpacing(10);
    contentLayout->addWidget(transaction1Div);
    contentLayout->addSpacing(10);
    contentLayout->addWidget(transaction2Frame);
    contentLayout->addSpacing(10);
    contentLayout->addWidget(transaction2Div);
    contentLayout->addSpacing(10);
    contentLayout->addWidget(upgradeFrame);
    contentLayout->addSpacing(10);
    contentLayout->addWidget(upgradeDiv);
    contentLayout->addSpacing(10);
    contentLayout->addWidget(rebuildFrame);
    contentLayout->addSpacing(10);
    contentLayout->addWidget(rebuildDiv);
    contentLayout->addSpacing(10);
    contentLayout->addStretch(1);

    layout->addWidget(scrollArea);

    connect(salvageWalletBtn, SIGNAL(clicked()), this, SLOT(walletSalvage()));
    connect(rescanBlockchainBtn, SIGNAL(clicked()), this, SLOT(walletRescan()));
    connect(transaction1Btn, SIGNAL(clicked()), this, SLOT(walletZaptxes1()));
    connect(transaction2Btn, SIGNAL(clicked()), this, SLOT(walletZaptxes2()));
    connect(upgradeBtn, SIGNAL(clicked()), this, SLOT(walletUpgrade()));
    connect(rebuildBtn, SIGNAL(clicked()), this, SLOT(walletReindex()));
}

void BlocknetWalletRepair::setWalletModel(WalletModel *w) {
    if (!walletModel)
        return;

    walletModel = w;
}

/** Restart wallet with "-salvagewallet" */
void BlocknetWalletRepair::walletSalvage()
{
    buildParameterlist(SALVAGEWALLET);
}

/** Restart wallet with "-rescan" */
void BlocknetWalletRepair::walletRescan()
{
    buildParameterlist(RESCAN);
}

/** Restart wallet with "-zapwallettxes=1" */
void BlocknetWalletRepair::walletZaptxes1()
{
    buildParameterlist(ZAPTXES1);
}

/** Restart wallet with "-zapwallettxes=2" */
void BlocknetWalletRepair::walletZaptxes2()
{
    buildParameterlist(ZAPTXES2);
}

/** Restart wallet with "-upgradewallet" */
void BlocknetWalletRepair::walletUpgrade()
{
    buildParameterlist(UPGRADEWALLET);
}

/** Restart wallet with "-reindex" */
void BlocknetWalletRepair::walletReindex()
{
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
    emit handleRestart(args);
}
