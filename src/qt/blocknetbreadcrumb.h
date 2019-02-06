// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETBREADCRUMB_H
#define BLOCKNETBREADCRUMB_H

#include <QFrame>
#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QButtonGroup>

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

signals:
    void crumbChanged(int crumb);

public slots:
    void goToCrumb(int crumb);

private:
    QHBoxLayout *layout;
    QVector<BlocknetCrumb> crumbs;
    QButtonGroup *group;
    int currentCrumb;
};

#endif // BLOCKNETBREADCRUMB_H