// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetkeydialog.h>

#include <qt/blocknetguiutil.h>

#include <QEvent>

BlocknetKeyDialog::BlocknetKeyDialog(QString highlight, QString note, QString action, QString title, QWidget *parent, Qt::WindowFlags flags) : 
                                        QDialog(parent, flags), layout(new QVBoxLayout) {
    this->setFixedSize(BGU::spi(500), BGU::spi(250));
    this->setLayout(layout);
    this->setModal(true);
    this->setContentsMargins(QMargins());

    layout->setContentsMargins(BGU::spi(50), BGU::spi(30), BGU::spi(50), BGU::spi(30));
    layout->setSpacing(BGU::spi(10));
    
    cancelBtn = new BlocknetCloseBtn;

    auto *cancelFrame = new QFrame;
    auto *cancelLayout = new QHBoxLayout;
    cancelFrame->setContentsMargins(QMargins());
    cancelLayout->setContentsMargins(0, 0, 0, 0);
    cancelFrame->setLayout(cancelLayout);
    cancelLayout->addStretch(1);
    cancelLayout->addWidget(cancelBtn, 0, Qt::AlignRight);

    titleLbl = new QLabel(title);
    titleLbl->setObjectName("h2");
    titleLbl->setAlignment(Qt::AlignCenter);

    highlightLbl = new QLabel(highlight);
    highlightLbl->setObjectName("highlight");
    highlightLbl->setAlignment(Qt::AlignCenter);

    noteLbl = new QLabel(note);
    noteLbl->setObjectName("note");
    noteLbl->setAlignment(Qt::AlignCenter);

    okBtn = new BlocknetFormBtn;
    okBtn->setText(action);
    
    auto *okFrame = new QFrame;
    auto *okLayout = new QHBoxLayout;
    okFrame->setContentsMargins(QMargins());
    okLayout->setContentsMargins(0, 0, 0, 0);
    okFrame->setLayout(okLayout);
    okLayout->addWidget(okBtn, 0, Qt::AlignCenter);

    layout->addWidget(cancelFrame);
    layout->addWidget(titleLbl);
    layout->addStretch(1);
    layout->addWidget(highlightLbl);
    layout->addWidget(noteLbl);
    layout->addStretch(1);
    layout->addWidget(okFrame);

    connect(okBtn, &QPushButton::clicked, this, [this]() {
        Q_EMIT accept();
    });
    connect(cancelBtn, &QPushButton::clicked, this, [this]() {
        Q_EMIT reject();
    });
}