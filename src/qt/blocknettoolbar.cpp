// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknettoolbar.h"

#include <QProgressBar>
#include <QTimer>
#include <QApplication>

#include <cmath>

BlocknetToolBar::BlocknetToolBar(QWidget *popup, QFrame *parent) : QFrame(parent), popupWidget(popup),
                                                                   layout(new QHBoxLayout) {
    layout->setAlignment(Qt::AlignRight);
    layout->setSpacing(18);
    this->setLayout(layout);

    peersIndicator = new BlocknetPeersIndicator;
    peersIndicator->setPeers(10);

    stakingIndicator = new BlocknetStakingIndicator;
    stakingIndicator->setObjectName("staking");

    progressIndicator = new QFrame;
    progressIndicator->setObjectName("progress");
    progressIndicator->setFixedSize(220, 28);
    progressIndicator->setLayout(new QHBoxLayout);
    progressIndicator->layout()->setContentsMargins(QMargins());
    progressBar = new QProgressBar;
#if defined(Q_OS_WIN)
    progressBar->setProperty("os", "win"); // work-around bug in Qt windows QProgressBar background
#endif
    progressBar->setAlignment(Qt::AlignVCenter);
    progressIndicator->layout()->addWidget(progressBar);

    lockIndicator = new BlocknetLockIndicator;
    lockIndicator->setObjectName("lock");

    layout->addStretch(1);
    layout->addWidget(peersIndicator);
    layout->addWidget(stakingIndicator);
    layout->addWidget(progressIndicator);
    layout->addWidget(lockIndicator);

    lockMenu = new BlocknetLockMenu;
    lockMenu->setDisplayWidget(popupWidget);
    lockMenu->hOnLockWallet = [&]() { emit lock(true); };
    lockMenu->hOnChangePw = [&]() { emit passphrase(); };
    lockMenu->hOnUnlockWallet = [&]() { emit lock(false); };
    lockMenu->hOnUnlockForStaking = [&]() { emit lock(false, true); };
    lockMenu->hOnTimedUnlock = [&]() { /*lockIndicator->setTime();*/ }; // TODO setTime
    lockMenu->hide();

    connect(lockIndicator, SIGNAL(lockRequest(bool)), this, SLOT(onLockClicked(bool)));
}

QLabel* BlocknetToolBar::getIcon(QString path, QString description, QSize size) {
    QPixmap pm(path);
    pm.setDevicePixelRatio(2); // TODO HDPI
    QLabel *icon = new QLabel(description);
    icon->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    icon->setFixedSize(size);
    icon->setPixmap(pm.scaled(icon->width()*pm.devicePixelRatio(), icon->height()*pm.devicePixelRatio(),
                              Qt::KeepAspectRatio, Qt::SmoothTransformation));
    return icon;
}

void BlocknetToolBar::setPeers(const int peers) {
    peersIndicator->setPeers(peers);
}

void BlocknetToolBar::setStaking(const bool on, const QString &msg) {
    stakingIndicator->setOn(on);
    stakingIndicator->setToolTip(msg);
}

void BlocknetToolBar::setLock(const bool lock, const bool stakingOnly) {
    lockIndicator->setLock(lock, stakingOnly);
}

void BlocknetToolBar::setProgress(const int progress, const QString &msg, const int maximum) {
    progressBar->setMaximum(maximum);
    progressBar->setValue(progress);
    progressBar->setStatusTip(msg);
    progressBar->setToolTip(msg.arg(""));
    progressBar->setFormat(QString("  %1").arg(msg));
}

void BlocknetToolBar::onLockClicked(bool lock) {
    if (lockMenu->isHidden()) {
        QPoint li = lockIndicator->mapToGlobal(QPoint());
        QPoint npos = popupWidget->mapFromGlobal(QPoint(li.x() - lockMenu->width() + 10, li.y() + lockIndicator->height() + 12));
        lockMenu->move(npos);
        lockMenu->show();
    }
}

BlocknetPeersIndicator::BlocknetPeersIndicator(QFrame *parent) : QFrame(parent), layout(new QHBoxLayout) {
    layout->setContentsMargins(QMargins());
    layout->setSpacing(4);
    this->setLayout(layout);

    auto *peersIcon = BlocknetToolBar::getIcon(QString(":/redesign/UtilityBar/Peers.png"), QString("Peers"), QSize(21, 20));
    peersLbl = new QLabel;
    peersLbl->setObjectName("peersLbl");

    layout->addWidget(peersIcon);
    layout->addWidget(peersLbl);
}

void BlocknetPeersIndicator::setPeers(const int peers) {
    peersLbl->setText(QString::number(peers));
    this->setToolTip(QString("%1: %2").arg(tr("Connected peers"), QString::number(peers)));
}

BlocknetStakingIndicator::BlocknetStakingIndicator(QFrame *parent) : QFrame(parent), layout(new QHBoxLayout) {
    layout->setContentsMargins(QMargins());
    layout->setSpacing(0);
    this->setLayout(layout);
    this->setOn(false);
}

void BlocknetStakingIndicator::setOn(const bool on) {
    if (stakingIcon != nullptr) {
        layout->removeWidget(stakingIcon);
        stakingIcon->deleteLater();
    }
    QString icon = on ? ":/redesign/UtilityBar/StakingNodeIconActive.png" :
                   ":/redesign/UtilityBar/StakingNodeIconInactive.png";
    stakingIcon = BlocknetToolBar::getIcon(icon, tr("Staking"), QSize(20, 20));
    layout->addWidget(stakingIcon);
}

/**
 * Manages the lock indicator. If the datetime is set, this icon will set a QTimer to check the time once per second.
 * @param parent
 */
BlocknetLockIndicator::BlocknetLockIndicator(QPushButton *parent) : QPushButton(parent), layout(new QHBoxLayout) {
    layout->setContentsMargins(QMargins());
    layout->setSpacing(0);
    layout->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
    this->setLayout(layout);
    this->setFixedSize(26, 24);
    this->setCursor(Qt::PointingHandCursor);
    this->setCheckable(true);
    this->setLock(false);
    connect(this, SIGNAL(clicked(bool)), this, SLOT(onClick(bool)));
}

/**
 * Sets the time. A timer is fired every 1 second. If the time is set to 0 epoch the timer is cleared.
 * @param time
 */
void BlocknetLockIndicator::setTime(const QDateTime time) {
    this->lockTime = time;

    // lock if time is in the future
    // TODO Needs work for older qt 5 versions
//    if (time.toSecsSinceEpoch() > QDateTime::currentSecsSinceEpoch())
//        this->setLock(true);
//
//    if (time.toSecsSinceEpoch() == 0) {
//        timer->stop();
//    } else {
//        this->lockTime = time;
//        if (timer == nullptr) {
//            timer = new QTimer;
//            timer->setInterval(1000);
//            timer->setTimerType(Qt::CoarseTimer);
//            connect(timer, SIGNAL(timeout()), this, SLOT(tick()));
//        }
//        timer->start();
//    }
}

void BlocknetLockIndicator::setLock(const bool locked, const bool stakingOnly) {
    this->locked = locked;
    this->stakingOnly = stakingOnly;
    removeLockIcon();

    QString icon = locked ? ":/redesign/UtilityBar/LockedIcon.png" :
                   ":/redesign/UtilityBar/UnlockedIcon.png";
    lockIcon = BlocknetToolBar::getIcon(icon, tr("Wallet lock state"), QSize(20, 16));
    layout->addWidget(lockIcon);

    if (this->locked) {
        this->setToolTip(this->stakingOnly ? tr("Locked for staking only") : tr("Wallet is locked"));
    } else {
        this->setToolTip(tr("Wallet is unlocked"));
    }
}

void BlocknetLockIndicator::tick() {
    // TODO Support older qt versions
//    QDateTime current = QDateTime::currentDateTime();
//    qint64 diff = lockTime.toSecsSinceEpoch() - current.toSecsSinceEpoch();
//    if (diff < 3600 && diff >= 0) { // 1 hr
//        if (lockIcon != nullptr)
//            lockIcon->hide();
//        if (elapsedLbl == nullptr) {
//            elapsedLbl = new QLabel;
//            elapsedLbl->setObjectName("timeLbl");
//            layout->addWidget(elapsedLbl);
//        }
//        qint64 t = lockTime.toSecsSinceEpoch() - current.toSecsSinceEpoch();
//        if (diff < 60) { // seconds
//            elapsedLbl->setText(QString::number(t) + "s");
//        } else { // minutes
//            elapsedLbl->setText(QString::number(ceil((double)t/60)) + "m");
//        }
//    } else if (elapsedLbl != nullptr) {
//        clearTimer();
//    }
}

void BlocknetLockIndicator::onClick(bool) {
    if (timer && timer->isActive()) {
        clearTimer();
    }

    // If locked send lock request
    emit lockRequest(!this->locked);
}

void BlocknetLockIndicator::removeLockIcon() {
    if (lockIcon == nullptr)
        return;
    layout->removeWidget(lockIcon);
    lockIcon->deleteLater();
    lockIcon = nullptr;
}

void BlocknetLockIndicator::clearTimer() {
    // TODO Support older qt versions
//    this->lockTime = QDateTime::fromSecsSinceEpoch(0);
//    if (elapsedLbl != nullptr) {
//        if (lockIcon != nullptr)
//            lockIcon->show();
//        layout->removeWidget(elapsedLbl);
//        elapsedLbl->deleteLater();
//        elapsedLbl = nullptr;
//        // now clear the timer
//        timer->stop();
//    }
}
