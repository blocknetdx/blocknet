// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETLOCKMENU_H
#define BLOCKNET_QT_BLOCKNETLOCKMENU_H

#include <functional>

#include <QFrame>
#include <QLayout>
#include <QPushButton>
#include <QSharedPointer>
#include <QVBoxLayout>

class BlocknetLockMenu : public QFrame
{
    Q_OBJECT
public:
    explicit BlocknetLockMenu(QFrame *parent = nullptr);
    ~BlocknetLockMenu() override;
    void setDisplayWidget(QWidget *widget);
    void show();

    std::function<void ()> hOnLockWallet = nullptr;
    std::function<void ()> hOnChangePw = nullptr;
    std::function<void ()> hOnUnlockWallet = nullptr;
    std::function<void ()> hOnUnlockForStaking = nullptr;
    std::function<void ()> hOnTimedUnlock = nullptr;

Q_SIGNALS:

public Q_SLOTS:
    void removeSelf(bool kill = true);

private Q_SLOTS:
    void onLockWallet();
    void onChangePw();
    void onUnlockWallet();
    void onUnlockForStaking();
    void onTimedUnlock();

private:
    QVBoxLayout *layout;
    QPushButton *lockWalletBtn;
    QPushButton *changePwBtn;
    QPushButton *unlockWalletBtn;
    QPushButton *unlockForStakingBtn;
    QPushButton *timedUnlockBtn;

    QWidget *displayWidget = nullptr;

    void setupBtn(QPushButton *btn);
    bool eventFilter(QObject *obj, QEvent *event) override;
};


#endif //BLOCKNET_QT_BLOCKNETLOCKMENU_H
