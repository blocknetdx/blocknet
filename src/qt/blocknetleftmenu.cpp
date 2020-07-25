// Copyright (c) 2018-2020 The Blocknet developers
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

BlocknetLeftMenu::BlocknetLeftMenu(QFrame *parent) : QFrame(parent), layout(new QVBoxLayout), selected(DASHBOARD) {
    this->setLayout(layout);
    this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    layout->setContentsMargins(0, BGU::spi(20), 0, 0);
    layout->setSpacing(BGU::spi(10));

    QPixmap pm(":/redesign/white_blocknet_logo.png");
    pm.setDevicePixelRatio(BGU::dpr());
    logo = new QLabel(tr("Blocknet Logo"));
    const auto lw = static_cast<qreal>(BGU::spi(150));
    const auto pw = static_cast<qreal>(pm.width());
    const auto ph = static_cast<qreal>(pm.height());
    logo->setFixedSize((int)lw, (int)(ph / pw * lw));
    logo->setPixmap(pm.scaled(logo->width()*pm.devicePixelRatio(), logo->height()*pm.devicePixelRatio(),
            Qt::KeepAspectRatio, Qt::SmoothTransformation));

    balanceLbl = new QLabel(tr("Available Balance:"));
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

    accounts = new BlocknetIconLabel;
    accounts->setIcon(":/redesign/Active/RequestFundsIcon.png", ":/redesign/Inactive/RequestFundsIcon.png");
    accounts->setLabel(tr("Balances"));

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
    group->addButton(accounts, ACCOUNTS);
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
        connect((BlocknetIconLabel*)btn, &BlocknetIconLabel::clicked, this, &BlocknetLeftMenu::onMenuClicked);

    versionLbl = new QLabel(QString::fromStdString(FormatFullVersion()));
    versionLbl->setObjectName("versionLbl");

    auto padding = QMargins(BGU::spi(10), 0, BGU::spi(10), 0);

    auto *box1 = new QFrame;
    box1->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    box1->setLayout(new QVBoxLayout);
    box1->setContentsMargins(padding);
    auto *boxBalance = new QFrame;
    boxBalance->setLayout(new QVBoxLayout);
    boxBalance->layout()->setContentsMargins(QMargins());
    boxBalance->layout()->setSpacing(BGU::spi(2));
    boxBalance->layout()->addWidget(balanceLbl);
    boxBalance->layout()->addWidget(balanceAmountLbl);
    dynamic_cast<QVBoxLayout*>(box1->layout())->addWidget(logo, 0, Qt::AlignLeft);
    dynamic_cast<QVBoxLayout*>(box1->layout())->addWidget(boxBalance, 0, Qt::AlignLeft);

    auto *box2 = new QFrame;
    box2->setContentsMargins(padding);
    box2->setLayout(new QVBoxLayout);
    box2->layout()->setSpacing(BGU::spi(8));
    box2->layout()->addWidget(dashboard);
    box2->layout()->addWidget(addressBook);
    box2->layout()->addWidget(accounts);

    auto *box3 = new QFrame;
    box3->setContentsMargins(padding);
    box3->setLayout(new QVBoxLayout);
    box3->layout()->setSpacing(BGU::spi(8));
    box3->layout()->addWidget(sendFunds);
//    box3->layout()->addWidget(requestFunds);
    box3->layout()->addWidget(transactionHistory);
    box3->layout()->addWidget(snodes);
    box3->layout()->addWidget(proposals);
//    box3->layout()->addWidget(announcements);

    auto *box4 = new QFrame;
    box4->setContentsMargins(padding);
    box4->setLayout(new QVBoxLayout);
    box4->layout()->setSpacing(BGU::spi(8));
    box4->layout()->addWidget(settings);
    box4->layout()->addWidget(tools);

    auto *boxVersion = new QFrame;
    boxVersion->setContentsMargins(padding);
    boxVersion->setLayout(new QVBoxLayout);
    boxVersion->layout()->addWidget(versionLbl);

    auto *div2 = new BlocknetHDiv;
    auto *div3 = new BlocknetHDiv;
    auto *div4 = new BlocknetHDiv;

    layout->addWidget(box1, 0, Qt::AlignCenter);
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
