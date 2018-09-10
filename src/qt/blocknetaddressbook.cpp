// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetaddressbook.h"
#include "blockneticonaltbtn.h"

BlocknetAddressBook::BlocknetAddressBook(WalletModel *w, QFrame *parent) : QFrame(parent), walletModel(w),
                                                                       layout(new QVBoxLayout) {
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->setContentsMargins(46, 10, 50, 0);
    this->setLayout(layout);

    titleLbl = new QLabel(tr("Address Book"));
    titleLbl->setObjectName("h4");
    titleLbl->setFixedHeight(26);

    auto *topBox = new QFrame;
    auto *topBoxLayout = new QHBoxLayout;
    topBoxLayout->setContentsMargins(QMargins());
    topBox->setLayout(topBoxLayout);

    auto *addAddressBtn = new BlocknetIconAltBtn(":/redesign/QuickActions/AddressButtonIcon.png");

    addButtonLbl = new QLabel(tr("Add Address"));
    addButtonLbl->setObjectName("h4");

    auto *createAddressBtn = new BlocknetIconAltBtn(":/redesign/QuickActions/ActionButtonIcon.png");

    createButtonLbl = new QLabel(tr("Create New Address"));
    createButtonLbl->setObjectName("h4");

    filterLbl = new QLabel(tr("Filter by:"));
    filterLbl->setObjectName("title");

    QStringList list{tr("All Addresses"), tr("Active"), tr("Upcoming"), tr("Completed")};
    addressDropdown = new BlocknetDropdown(list);

    topBoxLayout->addWidget(addAddressBtn, Qt::AlignLeft);
    topBoxLayout->addWidget(addButtonLbl, Qt::AlignLeft);
    topBoxLayout->addWidget(createAddressBtn, Qt::AlignLeft);
    topBoxLayout->addWidget(createButtonLbl, Qt::AlignLeft);
    topBoxLayout->addStretch(1);
    topBoxLayout->addWidget(filterLbl);
    topBoxLayout->addWidget(addressDropdown);

    layout->addWidget(titleLbl);
    layout->addWidget(topBox);
}

