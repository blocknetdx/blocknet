// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKMENU_H
#define BLOCKMENU_H

#include <QFrame>
#include <QLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QSharedPointer>

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

signals:

public slots:
    void removeSelf(bool kill = true);

private slots:
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


#endif //BLOCKMENU_H
