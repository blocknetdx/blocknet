// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETICONBTN_H
#define BLOCKNET_QT_BLOCKNETICONBTN_H

#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>

class BlocknetIconBtn : public QFrame
{
    Q_OBJECT
protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEvent *event) override {
        QWidget::enterEvent(event);
        hoverState = true;
        this->update();
    }
    void leaveEvent(QEvent *event) override {
        QWidget::leaveEvent(event);
        hoverState = false;
        this->update();
    }

    void mouseReleaseEvent(QMouseEvent *event) override;

public:
    explicit BlocknetIconBtn(const QString &title, const QString &img, QFrame *parent = nullptr);
    explicit BlocknetIconBtn(const QString &img, QFrame *parent = nullptr);
    QSize sizeHint() const override;

Q_SIGNALS:
    void clicked();

public Q_SLOTS:

private:
    const int circlew;
    const int circleh;
    bool hoverState;
    QLabel *icon;
    QLabel *iconLbl;
};

#endif // BLOCKNET_QT_BLOCKNETICONBTN_H