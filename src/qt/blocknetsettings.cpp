// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetsettings.h"

BlocknetSettings::BlocknetSettings(QWidget *parent) : QFrame(parent), layout(new QVBoxLayout) {
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->setContentsMargins(QMargins());
    this->setLayout(layout);

    content = new QFrame;
    content->setContentsMargins(37, 0, 40, 40);
    content->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum);
    content->setObjectName("contentFrame");
    contentLayout = new QVBoxLayout;
    content->setLayout(contentLayout);

    scrollArea = new QScrollArea;
    scrollArea->setContentsMargins(QMargins());
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setWidget(content);

    titleLbl = new QLabel(tr("Request Funds"));
    titleLbl->setObjectName("h4");

    auto *aboutBox = new QFrame;
    auto *aboutLayout = new QGridLayout;
    aboutLayout->setContentsMargins(QMargins());
    aboutLayout->setColumnStretch(0, 1);
    aboutLayout->setColumnStretch(1, 0);
    aboutBox->setLayout(aboutLayout);

    auto *aboutCoreLblBtn = new BlocknetLabelBtn;
    aboutCoreLblBtn->setText(tr("About BlocknetDX Core"));
    auto *aboutQtLblBtn = new BlocknetLabelBtn;
    aboutQtLblBtn->setText(tr("About Qt"));

    aboutLayout->addWidget(aboutCoreLblBtn, 0, 0, Qt::AlignRight);
    aboutLayout->addWidget(aboutQtLblBtn, 0, 1, Qt::AlignRight);

    generalLbl = new QLabel(tr("General"));
    generalLbl->setObjectName("h2");

    startWalletOnLoginCb = new QCheckBox(tr("Start wallet on system login"));
    startWalletOnLoginCb->setObjectName("subtractFeeCb");
    startWalletOnLoginCb->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    auto *generalBox = new QFrame;
    auto *generalBoxLayout = new QHBoxLayout;
    generalBoxLayout->setContentsMargins(QMargins());
    generalBox->setLayout(generalBoxLayout);

    sizeDbCacheLbl = new QLabel(tr("Size of database cache:"));
    sizeDbCacheLbl->setObjectName("title");

    QStringList cacheList{tr("100mb"), tr("Active"), tr("Upcoming"), tr("Completed")};
    dbCacheDropdown = new BlocknetDropdown(cacheList);

    verificationThreadsLbl = new QLabel(tr("Number of script verification threads:"));
    verificationThreadsLbl->setObjectName("title");

    QStringList threadList{tr("0"), tr("Active"), tr("Upcoming"), tr("Completed")};
    threadsDropdown = new BlocknetDropdown(threadList);

    generalBoxLayout->addWidget(sizeDbCacheLbl, 0, Qt::AlignLeft);
    generalBoxLayout->addWidget(dbCacheDropdown, 0, Qt::AlignLeft);
    generalBoxLayout->addStretch(1);
    generalBoxLayout->addWidget(verificationThreadsLbl);
    generalBoxLayout->addWidget(threadsDropdown);

    QLabel *generalDiv = new QLabel;
    generalDiv->setFixedHeight(1);
    generalDiv->setObjectName("hdiv");

    walletLbl = new QLabel(tr("Wallet"));
    walletLbl->setObjectName("h2");

    auto *walletGrid = new QFrame;
    walletGrid->setContentsMargins(QMargins());
    auto *walletLayout = new QGridLayout;
    walletLayout->setContentsMargins(QMargins());
    walletLayout->setColumnMinimumWidth(0, 200);
    walletGrid->setLayout(walletLayout);

    enableCoinCb = new QCheckBox(tr("Enable coin control features"));
    enableCoinCb->setObjectName("subtractFeeCb");
    enableCoinCb->setFixedWidth(400);

    spendChangeCb = new QCheckBox(tr("Spend unconfirmed change"));
    spendChangeCb->setObjectName("subtractFeeCb");
    spendChangeCb->setFixedWidth(300);

    walletLayout->addWidget(enableCoinCb, 0, 0, Qt::AlignLeft);
    walletLayout->addWidget(spendChangeCb, 0, 1, Qt::AlignLeft);

    auto *buttonGrid = new QFrame;
    buttonGrid->setContentsMargins(QMargins());
    auto *buttonLayout = new QGridLayout;
    buttonLayout->setContentsMargins(QMargins());
    buttonLayout->setColumnStretch(0, 0);
    buttonLayout->setColumnStretch(1, 2);
    buttonGrid->setLayout(buttonLayout);

    showBackupsBtn = new BlocknetFormBtn;
    showBackupsBtn->setText(tr("Show Automatic Backups"));
    backupBtn = new BlocknetFormBtn;
    backupBtn->setText(tr("Backup Wallet"));

    buttonLayout->addWidget(backupBtn, 0, 0, Qt::AlignLeft);
    buttonLayout->addWidget(showBackupsBtn, 0, 1, Qt::AlignLeft);

    QLabel *walletDiv = new QLabel;
    walletDiv->setFixedHeight(1);
    walletDiv->setObjectName("hdiv");

    networkLbl = new QLabel(tr("Network"));
    networkLbl->setObjectName("h2");

    auto *networkGrid = new QFrame;
    networkGrid->setContentsMargins(QMargins());
    auto *networkLayout = new QGridLayout;
    networkLayout->setContentsMargins(QMargins());
    networkLayout->setColumnMinimumWidth(0, 200);
    networkLayout->setRowMinimumHeight(0, 50);
    networkLayout->setRowMinimumHeight(1, 50);
    networkLayout->setRowMinimumHeight(2, 100);
    networkGrid->setLayout(networkLayout);

    mapPortCb = new QCheckBox(tr("Map port using UPnP"));
    mapPortCb->setObjectName("subtractFeeCb");
    mapPortCb->setFixedWidth(300);

    allowIncomingCb = new QCheckBox(tr("Allow incoming connections"));
    allowIncomingCb->setObjectName("subtractFeeCb");
    allowIncomingCb->setFixedWidth(300);

    connectSocks5Cb = new QCheckBox(tr("Connect through SOCKS5 proxy (default proxy)"));
    connectSocks5Cb->setObjectName("subtractFeeCb");
    connectSocks5Cb->setFixedWidth(320);

    proxyTi = new BlocknetLineEditWithTitle(tr("Proxy IP"), tr("Enter proxy ip address..."));
    proxyTi->setObjectName("address");
    proxyTi->setFixedWidth(250);

    portTi = new BlocknetLineEditWithTitle(tr("Port"), tr("Enter port number..."));
    portTi->setObjectName("address");
    portTi->setFixedWidth(250);

    networkLayout->addWidget(mapPortCb, 0, 0, Qt::AlignLeft);
    networkLayout->addWidget(allowIncomingCb, 0, 1, Qt::AlignLeft);
    networkLayout->addWidget(connectSocks5Cb, 1, 0, Qt::AlignLeft);
    networkLayout->addWidget(proxyTi, 2, 0, Qt::AlignLeft);
    networkLayout->addWidget(portTi, 2, 1, Qt::AlignLeft);

    QLabel *networkDiv = new QLabel;
    networkDiv->setFixedHeight(1);
    networkDiv->setObjectName("hdiv");

    displayLbl = new QLabel(tr("Display"));
    displayLbl->setObjectName("h2");

    auto *languageBox = new QFrame;
    languageBox->setContentsMargins(QMargins());
    auto *languageLayout = new QGridLayout;
    languageLayout->setContentsMargins(QMargins());
    languageLayout->setColumnStretch(0, 0);
    languageLayout->setColumnStretch(1, 1);
    languageBox->setLayout(languageLayout);

    languageLbl = new QLabel(tr("User interface language:"));
    languageLbl->setObjectName("title");

    QStringList languageList{tr("Default"), tr("Active"), tr("Upcoming"), tr("Completed")};
    languageDropdown = new BlocknetDropdown(languageList);

    languageLayout->addWidget(languageLbl, 0, 0, Qt::AlignLeft);
    languageLayout->addWidget(languageDropdown, 0, 1, Qt::AlignLeft);

    auto *translationBox = new QFrame;
    auto *translationLayout = new QGridLayout;
    translationLayout->setContentsMargins(QMargins());
    translationLayout->setColumnStretch(0, 0);
    translationLayout->setColumnStretch(1, 1);
    translationLayout->setSpacing(0);
    translationBox->setLayout(translationLayout);

    contributeLbl = new QLabel(tr("Language missing or translation incomplete? Help contributing translations"));
    contributeLbl->setObjectName("description");
    contributeLbl->setFixedWidth(425);

    auto *contributeLblBtn = new BlocknetLabelBtn;
    contributeLblBtn->setText(tr("here."));
    contributeLblBtn->setFixedWidth(48);

    translationLayout->addWidget(contributeLbl, 0, 0, Qt::AlignLeft);
    translationLayout->addWidget(contributeLblBtn, 0, 1, Qt::AlignLeft);

    auto *unitsBox = new QFrame;
    auto *unitsLayout = new QHBoxLayout;
    unitsLayout->setContentsMargins(QMargins());
    unitsBox->setLayout(unitsLayout);

    unitsLbl = new QLabel(tr("Units to show amounts in:"));
    unitsLbl->setObjectName("title");

    QStringList unitsList{tr("BLOCK"), tr("Active"), tr("Upcoming"), tr("Completed")};
    unitsDropdown = new BlocknetDropdown(unitsList);

    decimalLbl = new QLabel(tr("Decimal Digits:"));
    decimalLbl->setObjectName("title");

    QStringList decimalList{tr("2"), tr("Active"), tr("Upcoming"), tr("Completed")};
    decimalDropdown = new BlocknetDropdown(decimalList);

    unitsLayout->addWidget(unitsLbl, 0, Qt::AlignLeft);
    unitsLayout->addWidget(unitsDropdown, 0, Qt::AlignLeft);
    unitsLayout->addStretch(1);
    unitsLayout->addWidget(decimalLbl);
    unitsLayout->addWidget(decimalDropdown);

    thirdPartyUrlTi = new BlocknetLineEditWithTitle(tr("Third party transaction URLs"), tr("https://"));
    thirdPartyUrlTi->setObjectName("address");

    QLabel *displayDiv = new QLabel;
    displayDiv->setFixedHeight(1);
    displayDiv->setObjectName("hdiv");

    resetBtn = new BlocknetFormBtn;
    resetBtn->setText(tr("Reset to Default"));

    contentLayout->addWidget(titleLbl, 0, Qt::AlignTop | Qt::AlignLeft);
    contentLayout->addSpacing(1);
    contentLayout->addWidget(aboutBox);
    contentLayout->addSpacing(20);
    contentLayout->addWidget(generalLbl);
    contentLayout->addSpacing(20);
    contentLayout->addWidget(startWalletOnLoginCb);
    contentLayout->addSpacing(25);
    contentLayout->addWidget(generalBox);
    contentLayout->addSpacing(25);
    contentLayout->addWidget(generalDiv);
    contentLayout->addSpacing(25);
    contentLayout->addWidget(walletLbl);
    contentLayout->addSpacing(25);
    contentLayout->addWidget(walletGrid);
    contentLayout->addSpacing(20);
    contentLayout->addWidget(buttonGrid);
    contentLayout->addSpacing(25);
    contentLayout->addWidget(walletDiv);
    contentLayout->addSpacing(25);
    contentLayout->addWidget(networkLbl);
    contentLayout->addSpacing(10);
    contentLayout->addWidget(networkGrid);
    contentLayout->addWidget(networkDiv);
    contentLayout->addSpacing(20);
    contentLayout->addWidget(displayLbl);
    contentLayout->addSpacing(20);
    contentLayout->addWidget(languageBox);
    contentLayout->addSpacing(15);
    contentLayout->addWidget(translationBox);
    contentLayout->addSpacing(20);
    contentLayout->addWidget(unitsBox);
    contentLayout->addSpacing(20);
    contentLayout->addWidget(thirdPartyUrlTi);
    contentLayout->addSpacing(20);
    contentLayout->addWidget(displayDiv);
    contentLayout->addSpacing(45);
    contentLayout->addWidget(resetBtn);

    layout->addWidget(scrollArea);
}

void BlocknetSettings::setWalletModel(WalletModel *w) {
    if (walletModel == w)
        return;

    walletModel = w;
    if (!walletModel || !walletModel->getOptionsModel())
        return;
}