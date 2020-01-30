// Copyright (c) 2018-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetsendfundsdone.h>

#include <qt/blocknetguiutil.h>

BlocknetSendFundsDone::BlocknetSendFundsDone(QFrame *parent) : QFrame(parent), layout(new QVBoxLayout) {
//    this->setStyleSheet("border: 1px solid red");
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(layout);
    layout->setContentsMargins(BGU::spi(15), BGU::spi(10), BGU::spi(35), BGU::spi(30));

    titleLbl = new QLabel(tr("Send Funds"));
    titleLbl->setObjectName("h4");

    auto *subtitleLbl = new QLabel(tr("Your payment has been sent."));
    subtitleLbl->setObjectName("h2");

    auto *bodyLbl = new QLabel(tr("It may take several minutes to complete your transaction. You can view the status of your payment in your Transaction History."));
    bodyLbl->setObjectName("body");

    auto *hdiv = new QLabel;
    hdiv->setFixedHeight(1);
    hdiv->setObjectName("hdiv");

    returnBtn = new BlocknetFormBtn;
    returnBtn->setText(tr("Return to Dashboard"));

    sendBtn = new BlocknetFormBtn;
    sendBtn->setText(tr("Send Another Payment"));

    auto *btnBox = new QFrame;
    auto *btnBoxLayout = new QHBoxLayout;
    btnBoxLayout->setContentsMargins(QMargins());
    btnBoxLayout->setSpacing(BGU::spi(15));
    btnBox->setLayout(btnBoxLayout);
    btnBoxLayout->addWidget(returnBtn, 0, Qt::AlignLeft);
    btnBoxLayout->addWidget(sendBtn, 0, Qt::AlignLeft);
    btnBoxLayout->addStretch(1);

    layout->addWidget(titleLbl, 0, Qt::AlignTop | Qt::AlignLeft);
    layout->addSpacing(BGU::spi(45));
    layout->addWidget(subtitleLbl, 0, Qt::AlignTop);
    layout->addSpacing(BGU::spi(15));
    layout->addWidget(bodyLbl);
    layout->addSpacing(BGU::spi(35));
    layout->addWidget(hdiv);
    layout->addSpacing(BGU::spi(50));
    layout->addWidget(btnBox);
    layout->addStretch(1);

    connect(returnBtn, &BlocknetFormBtn::clicked, this, &BlocknetSendFundsDone::onReturnToDashboard);
    connect(sendBtn, &BlocknetFormBtn::clicked, this, &BlocknetSendFundsDone::onSendAnotherPayment);
}
