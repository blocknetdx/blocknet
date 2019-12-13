// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetlockmenu.h>

#include <qt/blocknetguiutil.h>

#include <QLabel>
#include <QEvent>
#include <QDebug>

BlocknetLockMenu::BlocknetLockMenu(QFrame *parent) : QFrame(parent), layout(new QVBoxLayout) {
    layout->setContentsMargins(QMargins());
    layout->setSpacing(0);
    this->setMaximumSize(BGU::spi(285), BGU::spi(290));
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
    hdivBox->setFixedHeight(BGU::spi(8));
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

    connect(lockWalletBtn, &QPushButton::clicked, this, &BlocknetLockMenu::onLockWallet);
    connect(changePwBtn, &QPushButton::clicked, this, &BlocknetLockMenu::onChangePw);
    connect(unlockWalletBtn, &QPushButton::clicked, this, &BlocknetLockMenu::onUnlockWallet);
    connect(unlockForStakingBtn, &QPushButton::clicked, this, &BlocknetLockMenu::onUnlockForStaking);
    connect(timedUnlockBtn, &QPushButton::clicked, this, &BlocknetLockMenu::onTimedUnlock);
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
    btn->setFixedHeight(BGU::spi(40));
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
