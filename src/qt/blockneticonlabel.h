// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETICONLABEL_H
#define BLOCKNET_QT_BLOCKNETICONLABEL_H

#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>

class BlocknetIconLabel : public QPushButton
{
    Q_OBJECT
public:
    explicit BlocknetIconLabel(QPushButton *parent = nullptr);
    ~BlocknetIconLabel() override;
    void setIcon(const QString active, const QString disabled);
    void setLabel(const QString &label);

protected:
    void paintEvent(QPaintEvent *e) override;

private Q_SLOTS:
    void onSelected(bool selected);

private:
    QHBoxLayout *layout;
    QLabel *label;
    QLabel *icon;
    QString iconActive;
    QString iconDisabled;
    QString labelText;
    bool *iconActiveState = nullptr;
};

#endif // BLOCKNET_QT_BLOCKNETICONLABEL_H
