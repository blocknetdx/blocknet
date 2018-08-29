// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetlockmenu.h"

#include <QLabel>
#include <QEvent>
#include <QDebug>

BlocknetLockMenu::BlocknetLockMenu(QFrame *parent) : QFrame(parent), layout(new QVBoxLayout) {
    layout->setContentsMargins(QMargins());
    layout->setSpacing(0);
    this->setMaximumSize(285, 290);
    this->setLayout(layout);

    lockWalletBtn = new QPushButton(tr("Lock Wallet"));
    changePwBtn = new QPushButton(tr("Change Password"));
    unlockWalletBtn = new QPushButton(tr("Unlock Wallet"));
    unlockForStakingBtn = new QPushButton(tr("Unlock Wallet for Staking Only"));
    timedUnlockBtn = new QPushButton(tr("Timed Unlock"));
    auto *hdiv = new QLabel;
    hdiv->setObjectName("hdiv");
    hdiv->setFixedHeight(1);
    auto *hdivBox = new QFrame;
    auto *hdivBoxLayout = new QVBoxLayout;
    hdivBox->setFixedHeight(8);
    hdivBox->setLayout(hdivBoxLayout);
    hdivBoxLayout->setContentsMargins(QMargins());
    hdivBoxLayout->addWidget(hdiv, 0, Qt::AlignVCenter);

    layout->addWidget(lockWalletBtn);
    layout->addWidget(changePwBtn);
    layout->addWidget(hdivBox);
    layout->addWidget(unlockWalletBtn);
    layout->addWidget(unlockForStakingBtn);
//    layout->addWidget(timedUnlockBtn);

    setupBtn(lockWalletBtn);
    setupBtn(changePwBtn);
    setupBtn(unlockWalletBtn);
    setupBtn(unlockForStakingBtn);
    setupBtn(timedUnlockBtn);

    connect(lockWalletBtn, SIGNAL(clicked()), this, SLOT(onLockWallet()));
    connect(changePwBtn, SIGNAL(clicked()), this, SLOT(onChangePw()));
    connect(unlockWalletBtn, SIGNAL(clicked()), this, SLOT(onUnlockWallet()));
    connect(unlockForStakingBtn, SIGNAL(clicked()), this, SLOT(onUnlockForStaking()));
    connect(timedUnlockBtn, SIGNAL(clicked()), this, SLOT(onTimedUnlock()));
}

void BlocknetLockMenu::show() {
    this->raise();
    QWidget::show();
}

void BlocknetLockMenu::setDisplayWidget(QWidget *widget) {
    if (!widget)
        return;
    this->setParent(widget);
    displayWidget = widget;
    displayWidget->installEventFilter(this);
}

void BlocknetLockMenu::onLockWallet() {
    if (hOnLockWallet)
        hOnLockWallet();
    removeSelf();
}

void BlocknetLockMenu::onChangePw() {
    if (hOnChangePw)
        hOnChangePw();
    removeSelf();
}

void BlocknetLockMenu::onUnlockWallet() {
    if (hOnUnlockWallet)
        hOnUnlockWallet();
    removeSelf();
}

void BlocknetLockMenu::onUnlockForStaking() {
    if (hOnUnlockForStaking)
        hOnUnlockForStaking();
    removeSelf();
}

void BlocknetLockMenu::onTimedUnlock() {
    if (hOnTimedUnlock)
        hOnTimedUnlock();
    removeSelf();
}

void BlocknetLockMenu::removeSelf(bool kill) {
    if ((!this->underMouse() || kill))
        this->hide();
}

void BlocknetLockMenu::setupBtn(QPushButton *btn) {
    btn->setFixedHeight(40);
    btn->setFlat(true);
    btn->setCursor(Qt::PointingHandCursor);
}

bool BlocknetLockMenu::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress && !this->isHidden() && !this->underMouse()) {
        removeSelf(false);
    }
    return QObject::eventFilter(obj, event);
}

BlocknetLockMenu::~BlocknetLockMenu() {
    hOnLockWallet = nullptr;
    hOnChangePw = nullptr;
    hOnUnlockWallet = nullptr;
    hOnUnlockForStaking = nullptr;
    hOnTimedUnlock = nullptr;
    displayWidget->removeEventFilter(this);
}
