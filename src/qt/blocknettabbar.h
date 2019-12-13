// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETTABBAR_H
#define BLOCKNET_QT_BLOCKNETTABBAR_H

#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

class BlocknetTabBar : public QFrame {
    Q_OBJECT
public:
    explicit BlocknetTabBar(QFrame *parent = nullptr);
    ~BlocknetTabBar() override;

    void addTab(QString title, int tab);
    int getTab() {
        return currentTab;
    }
    bool showTab(int tab);

    struct BlocknetTab {
        int tab;
        QString title;
    };

Q_SIGNALS:
    void tabChanged(int tab);

public Q_SLOTS:
    void goToTab(int tab);

private:
    QVBoxLayout *mainLayout;
    QHBoxLayout *layout;
    QVector<BlocknetTab> tabs;
    QButtonGroup *group;
    int currentTab;
};

#endif // BLOCKNET_QT_BLOCKNETTABBAR_H