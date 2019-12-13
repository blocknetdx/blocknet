// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetsettings.h>

#include <qt/blocknetcheckbox.h>
#include <qt/blocknetdialog.h>
#include <qt/blocknetguiutil.h>

#include <qt/bitcoinunits.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>

#include <qt/utilitydialog.h>
#include <netbase.h>
#include <txdb.h>

#include <validation.h>
#include <QApplication>
#include <QDir>
#include <QMessageBox>

BlocknetSettings::BlocknetSettings(interfaces::Node & node, QWidget *parent) : QFrame(parent),
                                                                               node(node),
                                                                               layout(new QVBoxLayout)
{
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->setContentsMargins(QMargins());
    this->setLayout(layout);

    content = new QFrame;
    content->setContentsMargins(BGU::spi(37), 0, BGU::spi(40), BGU::spi(40));
    content->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
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

    aboutCoreLblBtn = new BlocknetLabelBtn;
    aboutCoreLblBtn->setText(tr("About Blocknet"));
    aboutQtLblBtn = new BlocknetLabelBtn;
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

    startWalletOnLoginCb = new BlocknetCheckBox(tr("Start Blocknet on system login"));
    startWalletOnLoginCb->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    auto *generalGrid = new QFrame;
    generalGrid->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    generalGrid->setContentsMargins(QMargins());
    auto *generalBoxLayout = new QGridLayout;
    generalBoxLayout->setContentsMargins(QMargins());
    generalBoxLayout->setSizeConstraint(QLayout::SetMinimumSize);
    generalGrid->setLayout(generalBoxLayout);

    auto *sizeDbCacheBox = new QFrame;
    sizeDbCacheBox->setContentsMargins(QMargins());
    auto *sizeDbCacheLayout = new QHBoxLayout;
    sizeDbCacheLayout->setContentsMargins(QMargins());
    sizeDbCacheBox->setLayout(sizeDbCacheLayout);
    sizeDbCacheLbl = new QLabel(tr("Size of database cache (in megabytes):"));
    sizeDbCacheLbl->setObjectName("title");
    dbCacheSb = new QSpinBox;
    sizeDbCacheLayout->addWidget(sizeDbCacheLbl, 0, Qt::AlignLeft);
    sizeDbCacheLayout->addWidget(dbCacheSb, 0, Qt::AlignLeft);

    auto *verificationThreadsBox = new QFrame;
    verificationThreadsBox->setContentsMargins(QMargins());
    auto *verificationThreadsBoxLayout = new QHBoxLayout;
    verificationThreadsBoxLayout->setContentsMargins(QMargins());
    verificationThreadsBox->setLayout(verificationThreadsBoxLayout);
    verificationThreadsLbl = new QLabel(tr("Number of script verification threads:"));
    verificationThreadsLbl->setObjectName("title");
    threadsSb = new QSpinBox;
    verificationThreadsBoxLayout->addWidget(verificationThreadsLbl, 0, Qt::AlignLeft);
    verificationThreadsBoxLayout->addWidget(threadsSb, 0, Qt::AlignLeft);

    generalBoxLayout->addWidget(startWalletOnLoginCb, 0, 0, Qt::AlignLeft);
    generalBoxLayout->addWidget(sizeDbCacheBox, 1, 0, Qt::AlignLeft);
    generalBoxLayout->addWidget(verificationThreadsBox, 2, 0, Qt::AlignLeft);

    auto *generalDiv = new QLabel;
    generalDiv->setFixedHeight(1);
    generalDiv->setObjectName("hdiv");

    walletLbl = new QLabel(tr("Wallet"));
    walletLbl->setObjectName("h2");

    auto *walletGrid = new QFrame;
    walletGrid->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    walletGrid->setContentsMargins(QMargins());
    auto *walletLayout = new QGridLayout;
    walletLayout->setContentsMargins(QMargins());
    walletLayout->setColumnMinimumWidth(0, BGU::spi(200));
    walletGrid->setLayout(walletLayout);

    spendChangeCb = new BlocknetCheckBox(tr("Spend unconfirmed change"));
    walletLayout->addWidget(spendChangeCb, 0, 0, Qt::AlignLeft);

    auto *buttonGrid = new QFrame;
    buttonGrid->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    buttonGrid->setContentsMargins(QMargins());
    auto *buttonLayout = new QGridLayout;
    buttonLayout->setContentsMargins(QMargins());
    buttonLayout->setColumnStretch(0, 0);
    buttonLayout->setColumnStretch(1, 2);
    buttonGrid->setLayout(buttonLayout);

    backupBtn = new BlocknetFormBtn;
    backupBtn->setText(tr("Backup Wallet"));
    buttonLayout->addWidget(backupBtn, 0, 0, Qt::AlignLeft);

    auto *walletDiv = new QLabel;
    walletDiv->setFixedHeight(1);
    walletDiv->setObjectName("hdiv");

    networkLbl = new QLabel(tr("Network"));
    networkLbl->setObjectName("h2");

    auto *networkGrid = new QFrame;
    networkGrid->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    networkGrid->setContentsMargins(QMargins());
    auto *networkLayout = new QGridLayout;
    networkLayout->setContentsMargins(QMargins());
//    const int colWidth = BGU::spi(100);
//    networkLayout->setColumnMinimumWidth(0, colWidth);
//    networkLayout->setColumnMinimumWidth(1, colWidth);
//    networkLayout->setRowMinimumHeight(0, BGU::spi(35));
//    networkLayout->setRowMinimumHeight(1, BGU::spi(35));
//    networkLayout->setRowMinimumHeight(2, BGU::spi(50));
    networkGrid->setLayout(networkLayout);

    upnpCb = new BlocknetCheckBox(tr("Map port using UPnP"));
    allowIncomingCb = new BlocknetCheckBox(tr("Allow incoming connections"));
    connectSocks5Cb = new BlocknetCheckBox(tr("Connect through SOCKS5 proxy (default proxy)"));
//    upnpCb->setMinimumWidth(colWidth);
//    allowIncomingCb->setMinimumWidth(colWidth);
//    connectSocks5Cb->setMinimumWidth(colWidth*2); // spans 2 columns

    proxyTi = new BlocknetLineEditWithTitle(tr("Proxy IP"), tr("Enter proxy ip address..."), BGU::spi(100));
    proxyTi->setObjectName("address");
    proxyTi->lineEdit->setMaxLength(50);
    proxyTi->setEnabled(false); // default to disabled

    portTi = new BlocknetLineEditWithTitle(tr("Port"), tr("Enter port number..."), BGU::spi(100));
    portTi->lineEdit->setValidator(new QIntValidator(1, 65535, this));
    portTi->setObjectName("address");
    portTi->setEnabled(false); // default to disabled

    networkLayout->addWidget(upnpCb, 0, 0, Qt::AlignLeft);
    networkLayout->addWidget(allowIncomingCb, 0, 1, Qt::AlignLeft);
    networkLayout->addWidget(connectSocks5Cb, 1, 0, 1, 2, Qt::AlignLeft);
    networkLayout->addWidget(proxyTi, 2, 0, Qt::AlignLeft);
    networkLayout->addWidget(portTi, 2, 1, Qt::AlignLeft);

    auto *networkDiv = new QLabel;
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

    contributeLblBtn = new BlocknetLabelBtn;
    contributeLblBtn->setText(tr("here."));
    contributeLblBtn->setFixedWidth(48);

    translationLayout->addWidget(contributeLbl, 0, 0, Qt::AlignLeft);
    translationLayout->addWidget(contributeLblBtn, 0, 1, Qt::AlignLeft);

    auto *unitsBox = new QFrame;
    auto *unitsLayout = new QHBoxLayout;
    unitsLayout->setContentsMargins(QMargins());
    unitsLayout->setSizeConstraint(QLayout::SetMinimumSize);
    unitsBox->setLayout(unitsLayout);

    unitsLbl = new QLabel(tr("Units to show amounts in:"));
    unitsLbl->setObjectName("title");

    unitsDropdown = new BlocknetDropdown;
    unitsDropdown->setMaximumWidth(BGU::spi(100));
    unitsDropdown->setModel(new BitcoinUnits(this));

    decimalLbl = new QLabel(tr("Decimal digits:"));
    decimalLbl->setObjectName("title");

    decimalDropdown = new BlocknetDropdown;

    unitsLayout->addWidget(unitsLbl, 0, Qt::AlignLeft);
    unitsLayout->addWidget(unitsDropdown, 0, Qt::AlignLeft);
    unitsLayout->addStretch();
//    unitsLayout->addWidget(decimalLbl);
//    unitsLayout->addWidget(decimalDropdown); // TODO Blocknet Qt decimal dropdown

    thirdPartyUrlTi = new BlocknetLineEditWithTitle(tr("Third party transaction URLs"), tr("https://"));
    thirdPartyUrlTi->setObjectName("address");

    auto *displayDiv = new QLabel;
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
    contentLayout->addSpacing(BGU::spi(15));
    contentLayout->addWidget(generalLbl);
    contentLayout->addSpacing(BGU::spi(15));
    contentLayout->addWidget(generalGrid);
    contentLayout->addSpacing(BGU::spi(15));
    contentLayout->addWidget(generalDiv);
    contentLayout->addSpacing(BGU::spi(15));
    contentLayout->addWidget(walletLbl);
    contentLayout->addSpacing(BGU::spi(15));
    contentLayout->addWidget(walletGrid);
    contentLayout->addSpacing(BGU::spi(15));
    contentLayout->addWidget(buttonGrid);
    contentLayout->addSpacing(BGU::spi(15));
    contentLayout->addWidget(walletDiv);
    contentLayout->addSpacing(BGU::spi(15));
    contentLayout->addWidget(networkLbl);
    contentLayout->addSpacing(BGU::spi(10));
    contentLayout->addWidget(networkGrid);
    contentLayout->addSpacing(BGU::spi(15));
    contentLayout->addWidget(networkDiv);
    contentLayout->addSpacing(BGU::spi(15));
    contentLayout->addWidget(displayLbl);
//    contentLayout->addSpacing(BGU::spi(20));
//    contentLayout->addWidget(languageBox);
//    contentLayout->addSpacing(BGU::spi(15));
//    contentLayout->addWidget(translationBox);
    contentLayout->addSpacing(BGU::spi(15));
    contentLayout->addWidget(unitsBox);
    contentLayout->addSpacing(BGU::spi(15));
//    contentLayout->addWidget(thirdPartyUrlTi);
//    contentLayout->addSpacing(BGU::spi(20));
    contentLayout->addWidget(displayDiv);
    contentLayout->addSpacing(BGU::spi(35));
    contentLayout->addWidget(btnsBox, 0, Qt::AlignLeft);

    layout->addWidget(scrollArea);

    // Set initial states

    dbCacheSb->setMinimum(nMinDbCache);
    dbCacheSb->setMaximum(nMaxDbCache);

    threadsSb->setMinimum(0);
    threadsSb->setMaximum(MAX_SCRIPTCHECK_THREADS);

    for (int i = 2; i <= BitcoinUnits::decimals(BitcoinUnits::BTC); ++i)
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

    connect(backupBtn, &BlocknetFormBtn::clicked, this, [this]() { backupWallet(); });
    connect(aboutCoreLblBtn, &BlocknetFormBtn::clicked, this, [this]() {
        HelpMessageDialog dlg(this->node, this, true);
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
            if (!LookupNumeric(strAddrProxy.c_str(), addrProxy.GetPort()).IsValid()) {
                QMessageBox::warning(this, tr("Issue"), tr("The supplied proxy address is invalid."));
                return;
            }
            if (portTi->lineEdit->text().isEmpty()) {
                QMessageBox::warning(this, tr("Issue"), tr("The supplied proxy port is invalid."));
                return;
            }
        }
        if (mapper)
            mapper->submit();
        QMessageBox::information(this, tr("Restart Required"),
                tr("Please restart the wallet for changes to take effect."));
    });
    connect(connectSocks5Cb, &QCheckBox::toggled, proxyTi, &BlocknetLineEditWithTitle::setEnabled);
    connect(connectSocks5Cb, &QCheckBox::toggled, portTi, &BlocknetLineEditWithTitle::setEnabled);
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
    mapper->addMapping(languageDropdown, OptionsModel::Language);
    mapper->addMapping(unitsDropdown, OptionsModel::DisplayUnit);

    mapper->toFirst();
}

void BlocknetSettings::onResetSettingsToDefault() {
    BlocknetDialog dlg(tr("Are you sure you want to reset all your settings to default?\nThis will close the wallet."), tr("Reset to Default"), tr(""), this);
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

    if (!walletModel->wallet().backupWallet(filename.toStdString())) {
        QMessageBox::warning(this, tr("Backup Failed"),
                tr("There was an error trying to save the wallet data to %1.").arg(filename));
    } else {
        QMessageBox::information(this, tr("Backup Successful"),
                tr("The wallet data was successfully saved to %1.").arg(filename));
    }
}