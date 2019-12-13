// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetfundsmenu.h>

#include <qt/blocknetguiutil.h>

#include <QEvent>
#include <QLabel>

BlocknetFundsMenu::BlocknetFundsMenu(QFrame *parent) : QFrame(parent), layout(new QVBoxLayout) {
    layout->setContentsMargins(QMargins());
    layout->setSpacing(0);
    this->setMaximumSize(BGU::spi(160), BGU::spi(100));
    this->setLayout(layout);

    sendFundsBtn = new QPushButton(tr("Send Funds"));
    requestFundsBtn = new QPushButton(tr("Request Funds"));

    layout->addWidget(sendFundsBtn);
    layout->addWidget(requestFundsBtn);

    setupBtn(sendFundsBtn);
    setupBtn(requestFundsBtn);

//    connect(sendFundsBtn, &QPushButton::clicked, this, &BlocknetFundsMenu::onLockWallet);
//    connect(requestFundsBtn, &QPushButton::clicked, this, &BlocknetFundsMenu::onChangePw);
}

void BlocknetFundsMenu::show() {
    this->raise();
    QWidget::show();
}

void BlocknetFundsMenu::setDisplayWidget(QWidget *widget) {
    if (!widget)
        return;
    this->setParent(widget);
    displayWidget = widget;
    displayWidget->installEventFilter(this);
}

void BlocknetFundsMenu::onSendFunds() {
    if (hOnSendFunds)
        hOnSendFunds();
    removeSelf();
}

void BlocknetFundsMenu::onRequestFunds() {
    if (hOnRequestFunds)
        hOnRequestFunds();
    removeSelf();
}

void BlocknetFundsMenu::removeSelf(bool kill) {
    if ((!this->underMouse() || kill))
        this->hide();
}

void BlocknetFundsMenu::setupBtn(QPushButton *btn) {
    btn->setFixedHeight(BGU::spi(40));
    btn->setFlat(true);
    btn->setCursor(Qt::PointingHandCursor);
}

bool BlocknetFundsMenu::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress && !this->isHidden() && !this->underMouse()) {
        removeSelf(false);
    }
    return QObject::eventFilter(obj, event);
}

BlocknetFundsMenu::~BlocknetFundsMenu() {
    hOnSendFunds = nullptr;
    hOnRequestFunds = nullptr;
    displayWidget->removeEventFilter(this);
}
