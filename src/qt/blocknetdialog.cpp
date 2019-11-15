// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetdialog.h>

#include <qt/blocknetguiutil.h>

#include <QEvent>

BlocknetDialog::BlocknetDialog(QString message, QString actionMsg, QString actionStyle, QWidget *parent, Qt::WindowFlags flags) : QDialog(parent, flags), layout(new QVBoxLayout) {
    this->setFixedSize(BGU::spi(500), BGU::spi(250));
    this->setLayout(layout);
    this->setModal(true);

    layout->setContentsMargins(BGU::spi(50), BGU::spi(30), BGU::spi(50), BGU::spi(30));

    messageLbl = new QTextEdit(message);
    messageLbl->setObjectName("h3");
    messageLbl->setReadOnly(true);
    messageLbl->setTextInteractionFlags(Qt::NoTextInteraction);
    messageLbl->setAlignment(Qt::AlignCenter);
    messageLbl->setLineWrapMode(QTextEdit::WidgetWidth);
    layout->addWidget(messageLbl);

    auto *btnBox = new QFrame;
    btnBox->setObjectName("buttonSection");
    auto *btnBoxLayout = new QHBoxLayout;
    btnBox->setLayout(btnBoxLayout);

    cancelBtn = new BlocknetFormBtn;
    cancelBtn->setObjectName("cancel");
    cancelBtn->setText(tr("Cancel"));
    btnBoxLayout->addWidget(cancelBtn);

    deleteBtn = new BlocknetFormBtn;
    deleteBtn->setObjectName(actionStyle);
    deleteBtn->setText(actionMsg);
    btnBoxLayout->addWidget(deleteBtn);

    layout->addWidget(btnBox);

    connect(deleteBtn, &QPushButton::clicked, this, [this]() {
        Q_EMIT accept();
    });
    connect(cancelBtn, &QPushButton::clicked, this, [this]() {
        Q_EMIT reject();
    });
}