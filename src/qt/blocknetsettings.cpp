// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetsettings.h"
#include "blocknetdialog.h"

#include "txdb.h"
#include "bitcoinunits.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "utilitydialog.h"

#include <QApplication>
#include <QMessageBox>
#include <QDir>

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

    titleLbl = new QLabel(tr("Settings and Preferences"));
    titleLbl->setObjectName("h4");

    auto *aboutBox = new QFrame;
    auto *aboutLayout = new QHBoxLayout;
    aboutLayout->setContentsMargins(QMargins());
    aboutBox->setLayout(aboutLayout);

    auto *aboutCoreLblBtn = new BlocknetLabelBtn;
    aboutCoreLblBtn->setText(tr("About Blocknet"));
    auto *aboutQtLblBtn = new BlocknetLabelBtn;
    aboutQtLblBtn->setText(tr("About Qt"));

    aboutLayout->addWidget(aboutCoreLblBtn);
    aboutLayout->addWidget(aboutQtLblBtn);

    auto *titleBox = new QFrame;
    titleBox->setContentsMargins(QMargins());
    titleBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *titleBoxLayout = new QHBoxLayout;
    titleBoxLayout->setContentsMargins(QMargins());
    titleBox->setLayout(titleBoxLayout);
    titleBoxLayout->addWidget(titleLbl, 0, Qt::AlignLeft);
    titleBoxLayout->addStretch(1);
    titleBoxLayout->addWidget(aboutBox, 0, Qt::AlignBottom);

    generalLbl = new QLabel(tr("General"));
    generalLbl->setObjectName("h2");

    startWalletOnLoginCb = new QCheckBox(tr("Start Blocknet on system login"));
    startWalletOnLoginCb->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    auto *generalBox = new QFrame;
    auto *generalBoxLayout = new QHBoxLayout;
    generalBoxLayout->setContentsMargins(QMargins());
    generalBox->setLayout(generalBoxLayout);

    sizeDbCacheLbl = new QLabel(tr("Size of database cache (in megabytes):"));
    sizeDbCacheLbl->setObjectName("title");

    dbCacheSb = new QSpinBox;

    verificationThreadsLbl = new QLabel(tr("Number of script verification threads:"));
    verificationThreadsLbl->setObjectName("title");

    threadsSb = new QSpinBox;

    generalBoxLayout->addWidget(sizeDbCacheLbl, 0, Qt::AlignLeft);
    generalBoxLayout->addWidget(dbCacheSb, 0, Qt::AlignLeft);
    generalBoxLayout->addStretch(1);
    generalBoxLayout->addWidget(verificationThreadsLbl);
    generalBoxLayout->addWidget(threadsSb);

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

    spendChangeCb = new QCheckBox(tr("Spend unconfirmed change"));
    spendChangeCb->setFixedWidth(300);

    walletLayout->addWidget(spendChangeCb, 0, 0, Qt::AlignLeft);

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

    upnpCb = new QCheckBox(tr("Map port using UPnP"));
    upnpCb->setFixedWidth(300);

    allowIncomingCb = new QCheckBox(tr("Allow incoming connections"));
    allowIncomingCb->setFixedWidth(300);

    connectSocks5Cb = new QCheckBox(tr("Connect through SOCKS5 proxy (default proxy)"));
    connectSocks5Cb->setFixedWidth(320);

    proxyTi = new BlocknetLineEditWithTitle(tr("Proxy IP"), tr("Enter proxy ip address..."));
    proxyTi->setObjectName("address");
    proxyTi->setFixedWidth(250);
    proxyTi->setEnabled(false); // default to disabled

    portTi = new BlocknetLineEditWithTitle(tr("Port"), tr("Enter port number..."));
    portTi->lineEdit->setValidator(new QIntValidator(1, 65535, this));
    portTi->setObjectName("address");
    portTi->setFixedWidth(250);
    portTi->setEnabled(false); // default to disabled

    networkLayout->addWidget(upnpCb, 0, 0, Qt::AlignLeft);
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

    languageDropdown = new BlocknetDropdown;

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

    unitsDropdown = new BlocknetDropdown;
    unitsDropdown->setModel(new BitcoinUnits(this));

    decimalLbl = new QLabel(tr("Decimal Digits:"));
    decimalLbl->setObjectName("title");

    decimalDropdown = new BlocknetDropdown;

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

    saveBtn = new BlocknetFormBtn;
    saveBtn->setText(tr("Save Settings"));
    resetBtn = new BlocknetFormBtn;
    resetBtn->setObjectName("delete");
    resetBtn->setText(tr("Reset to Default"));

    auto *btnsBox = new QFrame;
    btnsBox->setContentsMargins(QMargins());
    auto *btnsBoxLayout = new QHBoxLayout;
    btnsBoxLayout->setContentsMargins(QMargins());
    btnsBox->setLayout(btnsBoxLayout);
    btnsBoxLayout->addWidget(saveBtn, 0, Qt::AlignLeft);
    btnsBoxLayout->addWidget(resetBtn, 0, Qt::AlignLeft);

    contentLayout->addWidget(titleBox);
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
//    contentLayout->addWidget(thirdPartyUrlTi);
//    contentLayout->addSpacing(20);
    contentLayout->addWidget(displayDiv);
    contentLayout->addSpacing(45);
    contentLayout->addWidget(btnsBox, 0, Qt::AlignLeft);

    layout->addWidget(scrollArea);

    // Set initial states

    dbCacheSb->setMinimum(nMinDbCache);
    dbCacheSb->setMaximum(nMaxDbCache);

    threadsSb->setMinimum(0);
    threadsSb->setMaximum(MAX_SCRIPTCHECK_THREADS);

    for (int i = 2; i <= BitcoinUnits::decimals(BitcoinUnits::BLOCK); ++i)
        decimalDropdown->addItem(QString::number(i), QString::number(i));

    QDir translations(":translations");
    languageDropdown->addItem(QString("(") + tr("default") + QString(")"), QVariant(""));
    for (const QString & langStr : translations.entryList()) {
        QLocale locale(langStr);
        // check if the locale name consists of 2 parts (language_country) */
        if (langStr.contains("_")) {
            // display language strings as "native language - native country (locale name)", e.g. "Deutsch - Deutschland (de)" */
            languageDropdown->addItem(locale.nativeLanguageName() + QString(" - ") + locale.nativeCountryName() + QString(" (") + langStr + QString(")"), QVariant(langStr));
        } else {
            // display language strings as "native language (locale name)", e.g. "Deutsch (de)" */
            languageDropdown->addItem(locale.nativeLanguageName() + QString(" (") + langStr + QString(")"), QVariant(langStr));
        }
    }

#ifndef USE_UPNP
    upnpCb->setEnabled(false);
#endif

    connect(showBackupsBtn, &BlocknetFormBtn::clicked, this, [this]() { GUIUtil::showBackups(); });
    connect(backupBtn, &BlocknetFormBtn::clicked, this, [this]() { backupWallet(); });
    connect(aboutCoreLblBtn, &BlocknetFormBtn::clicked, this, [this]() {
        HelpMessageDialog dlg(this, true);
        dlg.setStyleSheet(GUIUtil::loadStyleSheetv1());
        dlg.exec();
    });
    connect(aboutQtLblBtn, &BlocknetFormBtn::clicked, this, [this]() {
        qApp->aboutQt();
    });
    connect(resetBtn, &BlocknetFormBtn::clicked, this, &BlocknetSettings::onResetSettingsToDefault);
    connect(saveBtn, &BlocknetFormBtn::clicked, this, [this]() {
        if (connectSocks5Cb->isChecked()) {
            const std::string strAddrProxy = proxyTi->lineEdit->text().toStdString();
            CService addrProxy;
            // Check for a valid IPv4 / IPv6 address
            if (!LookupNumeric(strAddrProxy.c_str(), addrProxy)) {
                QMessageBox::warning(this, tr("Issue"), tr("The supplied proxy address is invalid."));
                return;
            }
            if (portTi->lineEdit->text().isEmpty()) {
                QMessageBox::warning(this, tr("Issue"), tr("The supplied proxy port is invalid."));
                return;
            }
        }
        mapper->submit();
        pwalletMain->MarkDirty();
        QMessageBox::information(this, tr("Restart Required"),
                tr("Please restart the wallet for changes to take affect."));
    });
    connect(connectSocks5Cb, SIGNAL(toggled(bool)), proxyTi, SLOT(setEnabled(bool)));
    connect(connectSocks5Cb, SIGNAL(toggled(bool)), portTi, SLOT(setEnabled(bool)));
}

void BlocknetSettings::setWalletModel(WalletModel *w) {
    if (walletModel == w)
        return;

    walletModel = w;
    if (!walletModel || !walletModel->getOptionsModel())
        return;

    mapper = new QDataWidgetMapper(this);
    mapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);
    mapper->setOrientation(Qt::Vertical);
    mapper->setModel(walletModel->getOptionsModel());

    // Main
    mapper->addMapping(startWalletOnLoginCb, OptionsModel::StartAtStartup);
    mapper->addMapping(threadsSb, OptionsModel::ThreadsScriptVerif);
    mapper->addMapping(dbCacheSb, OptionsModel::DatabaseCache);
    // Wallet
    mapper->addMapping(spendChangeCb, OptionsModel::SpendZeroConfChange);
    // Network
    mapper->addMapping(upnpCb, OptionsModel::MapPortUPnP);
    mapper->addMapping(allowIncomingCb, OptionsModel::Listen);
    mapper->addMapping(connectSocks5Cb, OptionsModel::ProxyUse);
    mapper->addMapping(proxyTi, OptionsModel::ProxyIP);
    mapper->addMapping(portTi, OptionsModel::ProxyPort);
    // Display
    mapper->addMapping(decimalDropdown, OptionsModel::Digits);
    mapper->addMapping(languageDropdown, OptionsModel::Language);
    mapper->addMapping(unitsDropdown, OptionsModel::DisplayUnit);

    mapper->toFirst();
}

void BlocknetSettings::onResetSettingsToDefault() {
    BlocknetDialog dlg(tr("Are you sure you want to reset all your settings to default?\nThis will close the wallet."), tr("Reset to Default"), tr(""), this);
    dlg.setFixedSize(500, 380);
    connect(&dlg, &QDialog::accepted, this, [this]() {
        walletModel->getOptionsModel()->Reset();
        QApplication::quit();
    });
    dlg.exec();
}

void BlocknetSettings::backupWallet() {
    QString filename = GUIUtil::getSaveFileName(this, tr("Backup Wallet"), QString(),
                                                tr("Wallet Data (*.dat)"), nullptr);

    if (filename.isEmpty())
        return;

    if (!walletModel->backupWallet(filename)) {
        QMessageBox::warning(this, tr("Backup Failed"),
                tr("There was an error trying to save the wallet data to %1.").arg(filename));
    } else {
        QMessageBox::information(this, tr("Backup Successful"),
                tr("The wallet data was successfully saved to %1.").arg(filename));
    }
}