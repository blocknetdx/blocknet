// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETTOOLBAR_H
#define BLOCKNETTOOLBAR_H

#include "blocknetlockmenu.h"

#include <QFrame>
#include <QPushButton>
#include <QHBoxLayout>
#include <QBoxLayout>
#include <QDateTime>
#include <QLabel>
#include <QProgressBar>

class BlocknetPeersIndicator : public QFrame
{
Q_OBJECT
public:
    explicit BlocknetPeersIndicator(QFrame *parent = nullptr);
    void setPeers(int peers);

signals:

public slots:

private:
    QHBoxLayout *layout;
    QLabel *peersLbl;
};

class BlocknetStakingIndicator : public QFrame
{
Q_OBJECT
public:
    explicit BlocknetStakingIndicator(QFrame *parent = nullptr);
    void setOn(bool on);

signals:

public slots:

private:
    QHBoxLayout *layout;
    QLabel *stakingIcon = nullptr;
};

class BlocknetLockIndicator : public QPushButton
{
Q_OBJECT
public:
    explicit BlocknetLockIndicator(QPushButton *parent = nullptr);
    void setTime(QDateTime time);
    void setLock(bool locked, bool stakingOnly = false);

signals:
    void lockRequest(bool wantLock);

private slots:
    void tick();
    void onClick(bool checked);

private:
    bool locked;
    bool stakingOnly;
    QHBoxLayout *layout;
    QLabel *lockIcon = nullptr;
    QLabel *elapsedLbl = nullptr;
    QDateTime lockTime;
    QTimer *timer = nullptr;

    void removeLockIcon();
    void clearTimer();
};

class BlocknetToolBar : public QFrame
{
Q_OBJECT
public:
    explicit BlocknetToolBar(QWidget *popups, QFrame *parent = nullptr);
    void setPeers(int peers);
    void setStaking(bool on, const QString &msg);
    void setLock(bool lock, bool stakingOnly);
    void setProgress(int progress, const QString &msg, int maximum);
    static QLabel* getIcon(QString path, QString description, QSize size);

signals:
    void lock(bool locked, bool stakingOnly = false);
    void passphrase();

public slots:
    void onLockClicked(bool lock);

private:
    QHBoxLayout *layout;
    BlocknetPeersIndicator *peersIndicator;
    BlocknetStakingIndicator *stakingIndicator;
    QFrame *progressIndicator;
    QProgressBar *progressBar;
    BlocknetLockIndicator *lockIndicator;
    BlocknetLockMenu *lockMenu = nullptr;
    QWidget *popupWidget;
};

#endif // BLOCKNETTOOLBAR_H
