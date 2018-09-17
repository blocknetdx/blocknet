// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKFUNDSMENU_H
#define BLOCKFUNDSMENU_H

#include <functional>

#include <QFrame>
#include <QLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QSharedPointer>

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

signals:

public slots:
    void removeSelf(bool kill = true);

private slots:
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


#endif //BLOCKFUNDSMENU_H
