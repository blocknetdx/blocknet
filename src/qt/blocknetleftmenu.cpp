// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetleftmenu.h>

#include <qt/blocknethdiv.h>
#include <qt/blocknetguiutil.h>
#include <qt/blocknetvars.h>

#include <qt/bitcoinunits.h>

#include <clientversion.h>

#include <QButtonGroup>
#include <QSettings>
#include <QSizePolicy>

BlocknetLeftMenu::BlocknetLeftMenu(QFrame *parent) : QFrame(parent), layout(new QVBoxLayout) {
    this->setLayout(layout);
    layout->setContentsMargins(0, BGU::spi(20), 0, 0);
    layout->setSpacing(BGU::spi(10));
    this->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);

    QPixmap pm(":/redesign/white_blocknet_logo.png");
    pm.setDevicePixelRatio(BGU::dpr());
    logo = new QLabel(tr("Blocknet Logo"));
    logo->setFixedHeight(BGU::spi(30));
    logo->setPixmap(pm.scaledToHeight(logo->height()*pm.devicePixelRatio(), Qt::SmoothTransformation));

    balanceLbl = new QLabel(tr("Total Balance:"));
    balanceLbl->setObjectName("balanceLbl");
    balanceAmountLbl = new QLabel;
    balanceAmountLbl->setObjectName("balanceAmountLbl");

    dashboard = new BlocknetIconLabel;
    dashboard->setDefault(true);
    dashboard->setIcon(":/redesign/Active/DashboardIcon.png", ":/redesign/Inactive/DashboardIcon.png");
    dashboard->setLabel(tr("Dashboard"));

    addressBook = new BlocknetIconLabel;
    addressBook->setIcon(":/redesign/Active/AddressBookIcon.png", ":/redesign/Inactive/AddressBookIcon.png");
    addressBook->setLabel(tr("Address Book"));

    sendFunds = new BlocknetIconLabel;
    sendFunds->setIcon(":/redesign/Active/SendFundsIcon.png", ":/redesign/Inactive/SendFundsIcon.png");
    sendFunds->setLabel(tr("Send Funds"));

    requestFunds = new BlocknetIconLabel;
    requestFunds->setIcon(":/redesign/Active/RequestFundsIcon.png", ":/redesign/Inactive/RequestFundsIcon.png");
    requestFunds->setLabel(tr("Request Funds"));

    transactionHistory = new BlocknetIconLabel;
    transactionHistory->setIcon(":/redesign/Active/TransactionHistoryIcon.png", ":/redesign/Inactive/TransactionHistoryIcon.png");
    transactionHistory->setLabel(tr("Transaction History"));

    snodes = new BlocknetIconLabel;
    snodes->setIcon(":/redesign/Active/ServiceNodesIcon.png", ":/redesign/Inactive/ServiceNodesIcon.png");
    snodes->setLabel(tr("Service Nodes"));

    proposals = new BlocknetIconLabel;
    proposals->setIcon(":/redesign/Active/ProposalIcon.png", ":/redesign/Inactive/ProposalIcon.png");
    proposals->setLabel(tr("Proposals"));

    announcements = new BlocknetIconLabel;
    announcements->setIcon(":/redesign/Active/AnnouncementsIcon.png", ":/redesign/Inactive/AnnouncementsIcon.png");
    announcements->setLabel(tr("Announcements"));

    settings = new BlocknetIconLabel;
    settings->setIcon(":/redesign/Active/SettingsIcon.png", ":/redesign/Inactive/SettingsIcon.png");
    settings->setLabel(tr("Settings"));

    tools = new BlocknetIconLabel;
    tools->setIcon(":/redesign/Active/ToolsIcon.png", ":/redesign/Inactive/ToolsIcon.png");
    tools->setLabel(tr("Tools"));

    group = new QButtonGroup;
    group->setExclusive(false);
    group->addButton(dashboard, DASHBOARD);
    group->addButton(addressBook, ADDRESSBOOK);
    group->addButton(sendFunds, SEND);
//    group->addButton(requestFunds, REQUEST);
    group->addButton(transactionHistory, HISTORY);
    group->addButton(snodes, SNODES);
    group->addButton(proposals, PROPOSALS);
//    group->addButton(announcements, ANNOUNCEMENTS);
    group->addButton(settings, SETTINGS);
    group->addButton(tools, TOOLS);

    // Manually handle the button events
    btns = group->buttons();
    for (auto *btn : btns)
        connect(btn, SIGNAL(clicked(bool)), this, SLOT(onMenuClicked(bool)));

    versionLbl = new QLabel(QString::fromStdString(FormatFullVersion()));
    versionLbl->setObjectName("versionLbl");

    auto leftPadding = QMargins(BGU::spi(10), 0, 0, 0);

    auto *box1 = new QFrame;
    box1->setLayout(new QVBoxLayout);
    box1->setContentsMargins(leftPadding);
    auto *boxLogo = new QFrame;
    boxLogo->setLayout(new QVBoxLayout);
    boxLogo->layout()->setContentsMargins(QMargins());
    boxLogo->layout()->setSpacing(BGU::spi(14));
    boxLogo->layout()->addWidget(logo);
    auto *boxBalance = new QFrame;
    boxBalance->setLayout(new QVBoxLayout);
    boxBalance->layout()->setContentsMargins(QMargins());
    boxBalance->layout()->setSpacing(BGU::spi(2));
    boxBalance->layout()->addWidget(balanceLbl);
    boxBalance->layout()->addWidget(balanceAmountLbl);
    box1->layout()->addWidget(boxLogo);
    box1->layout()->addWidget(boxBalance);

    auto *box2 = new QFrame;
    box2->setContentsMargins(leftPadding);
    box2->setLayout(new QVBoxLayout);
    box2->layout()->setSpacing(BGU::spi(8));
    box2->layout()->addWidget(dashboard);
    box2->layout()->addWidget(addressBook);

    auto *box3 = new QFrame;
    box3->setContentsMargins(leftPadding);
    box3->setLayout(new QVBoxLayout);
    box3->layout()->setSpacing(BGU::spi(8));
    box3->layout()->addWidget(sendFunds);
//    box3->layout()->addWidget(requestFunds);
    box3->layout()->addWidget(transactionHistory);
    QSettings qSettings;
    if (qSettings.value("fShowServicenodesTab").toBool()) {
        box3->layout()->addWidget(snodes);
    }
    box3->layout()->addWidget(proposals);
//    box3->layout()->addWidget(announcements);

    auto *box4 = new QFrame;
    box4->setContentsMargins(leftPadding);
    box4->setLayout(new QVBoxLayout);
    box4->layout()->setSpacing(BGU::spi(8));
    box4->layout()->addWidget(settings);
    box4->layout()->addWidget(tools);

    auto *boxVersion = new QFrame;
    boxVersion->setContentsMargins(leftPadding);
    boxVersion->setLayout(new QVBoxLayout);
    boxVersion->layout()->addWidget(versionLbl);

    auto *div2 = new BlocknetHDiv;
    auto *div3 = new BlocknetHDiv;
    auto *div4 = new BlocknetHDiv;

    layout->addWidget(box1);
    layout->addWidget(div2);
    layout->addWidget(box2);
    layout->addWidget(div3);
    layout->addWidget(box3);
    layout->addWidget(div4);
    layout->addWidget(box4);
    layout->addStretch(1);
    layout->addWidget(boxVersion);

    announcements->setObjectName("disabled"); announcements->setEnabled(false); announcements->setToolTip(tr("Coming soon"));
}

void BlocknetLeftMenu::setBalance(CAmount balance, int unit) {
    balanceAmountLbl->setText(BitcoinUnits::floorWithUnit(unit, balance, 2, false, BitcoinUnits::separatorAlways));
}

void BlocknetLeftMenu::selectMenu(const BlocknetPage menuType) {
    selected = menuType;
    for (auto *btn : btns) {
        auto btnID = group->id(btn);
        if (btnID == DASHBOARD && menuType == QUICKSEND) // quicksend is alias of dashboard // TODO Handle dynamically
            btn->setChecked(true);
        else btn->setChecked(btnID == menuType);
    }
}

void BlocknetLeftMenu::onMenuClicked(bool) {
    auto *btn = qobject_cast<QAbstractButton*>(sender());
    // Always allow going to the dashboard
    auto page = static_cast<BlocknetPage>(group->id(btn));
    if (selected != page || page == DASHBOARD) {
        Q_EMIT menuChanged(page);
    }
}
