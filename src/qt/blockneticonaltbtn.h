// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETICONALTBTN_H
#define BLOCKNETICONALTBTN_H

#include <QFrame>
#include <QVBoxLayout>
#include <QLabel>

class BlocknetIconAltBtn : public QFrame
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
    explicit BlocknetIconAltBtn(const QString &img, int padding = 0, QFrame *parent = nullptr);
    QSize sizeHint() const override;

signals:
    void clicked();

public slots:

private:
    const int circlew = 84;
    const int circleh = circlew;
    QVBoxLayout *layout;
    bool hoverState = false;
};

#endif // BLOCKNETICONALTBTN_H