// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <QEvent>
#include "blocknetdialog.h"

BlocknetDialog::BlocknetDialog(QString message, QString actionMsg, QWidget *parent, Qt::WindowFlags flags) : QDialog(parent, flags), layout(new QVBoxLayout) {
    this->setFixedSize(500, 250);
    this->setLayout(layout);
    this->setModal(true);

    layout->setContentsMargins(50, 30, 50, 30);

    messageLbl = new QTextEdit(message);
    messageLbl->setObjectName("h2");
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
    deleteBtn->setObjectName("delete");
    deleteBtn->setText(actionMsg);
    btnBoxLayout->addWidget(deleteBtn);

    layout->addWidget(btnBox);

    connect(deleteBtn, &QPushButton::clicked, this, [this]() {
        emit accept();
    });
    connect(cancelBtn, &QPushButton::clicked, this, [this]() {
        emit reject();
    });
}