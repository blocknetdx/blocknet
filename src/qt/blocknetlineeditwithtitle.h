// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETLINEEDITWITHTITLE_H
#define BLOCKNET_QT_BLOCKNETLINEEDITWITHTITLE_H

#include <qt/blocknetlineedit.h>

#include <QBoxLayout>
#include <QFrame>
#include <QLabel>

class BlocknetLineEditWithTitle : public QFrame
{
    Q_OBJECT
public:
    explicit BlocknetLineEditWithTitle(QString title = "", QString placeholder = "",
            int w = BGU::spi(250), QFrame *parent = nullptr);
    void setID(QString id);
    void setError(bool flag = true);
    void setTitle(const QString &title);
    QString getID();
    bool isEmpty();
    QSize sizeHint() const override;
    BlocknetLineEdit *lineEdit;
    void setExpanding();

Q_SIGNALS:

public Q_SLOTS:

private:
    QString id;
    QLabel *titleLbl;
    QVBoxLayout *layout;
};

#endif // BLOCKNET_QT_BLOCKNETLINEEDITWITHTITLE_H
