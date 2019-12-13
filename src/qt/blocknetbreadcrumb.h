// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETBREADCRUMB_H
#define BLOCKNET_QT_BLOCKNETBREADCRUMB_H

#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>

class BlocknetArrow : public QWidget {
    Q_OBJECT
public:
    explicit BlocknetArrow(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};

class BlocknetBreadCrumb : public QFrame {
    Q_OBJECT
public:
    explicit BlocknetBreadCrumb(QFrame *parent = nullptr);
    ~BlocknetBreadCrumb() override;
    QSize sizeHint() const override;
    void addCrumb(QString title, int crumb);
    int getCrumb() {
        return currentCrumb;
    }
    bool showCrumb(int crumb);

    struct BlocknetCrumb {
        int crumb;
        QString title;
    };

Q_SIGNALS:
    void crumbChanged(int crumb);

public Q_SLOTS:
    void goToCrumb(int crumb);

private:
    QHBoxLayout *layout;
    QVector<BlocknetCrumb> crumbs;
    QButtonGroup *group;
    int currentCrumb;
};

#endif // BLOCKNET_QT_BLOCKNETBREADCRUMB_H