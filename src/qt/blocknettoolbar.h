// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETTOOLBAR_H
#define BLOCKNET_QT_BLOCKNETTOOLBAR_H

#include <qt/blocknetlockmenu.h>

#include <QBoxLayout>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>

class BlocknetPeersIndicator : public QFrame
{
Q_OBJECT
public:
    explicit BlocknetPeersIndicator(QFrame *parent = nullptr);
    void setPeers(int peers);

Q_SIGNALS:

public Q_SLOTS:

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

Q_SIGNALS:

public Q_SLOTS:

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

Q_SIGNALS:
    void lockRequest(bool wantLock);

private Q_SLOTS:
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

Q_SIGNALS:
    void lock(bool locked, bool stakingOnly = false);
    void passphrase();
    void progressClicked();

public Q_SLOTS:
    void onLockClicked(bool lock);

protected:
    bool eventFilter(QObject *object, QEvent *event);

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

#endif // BLOCKNET_QT_BLOCKNETTOOLBAR_H
