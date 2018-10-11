// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETTABBAR_H
#define BLOCKNETTABBAR_H

#include <QFrame>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QButtonGroup>

class BlocknetTabBar : public QFrame {
    Q_OBJECT
public:
    explicit BlocknetTabBar(QFrame *parent = nullptr);
    ~BlocknetTabBar() override;
    QSize sizeHint() const override;
    void addTab(QString title, int tab);
    int getTab() {
        return currentTab;
    }
    bool showTab(int tab);

    struct BlocknetTab {
        int tab;
        QString title;
    };

signals:
    void tabChanged(int tab);

public slots:
    void goToTab(int tab);

private:
    QVBoxLayout *mainLayout;
    QHBoxLayout *layout;
    QVector<BlocknetTab> tabs;
    QButtonGroup *group;
    int currentTab;
};

#endif // BLOCKNETTABBAR_H