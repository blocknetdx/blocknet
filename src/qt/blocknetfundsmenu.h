// Copyright (c) 2018-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETFUNDSMENU_H
#define BLOCKNET_QT_BLOCKNETFUNDSMENU_H

#include <functional>

#include <QFrame>
#include <QLayout>
#include <QPushButton>
#include <QSharedPointer>
#include <QVBoxLayout>

class BlocknetFundsMenu : public QFrame
{
    Q_OBJECT
public:
    explicit BlocknetFundsMenu(QFrame *parent = nullptr);
    ~BlocknetFundsMenu() override;
    void setDisplayWidget(QWidget *widget);
    void show();

    std::function<void ()> hOnSendFunds = nullptr;
    std::function<void ()> hOnRequestFunds = nullptr;

Q_SIGNALS:

public Q_SLOTS:
    void removeSelf(bool kill = true);

private Q_SLOTS:
    void onSendFunds();
    void onRequestFunds();

private:
    QVBoxLayout *layout;
    QPushButton *sendFundsBtn;
    QPushButton *requestFundsBtn;
    QWidget *displayWidget = nullptr;

    void setupBtn(QPushButton *btn);
    bool eventFilter(QObject *obj, QEvent *event) override;
};


#endif //BLOCKNET_QT_BLOCKNETFUNDSMENU_H
