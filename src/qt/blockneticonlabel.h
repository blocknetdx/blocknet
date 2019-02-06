// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETICONLABEL_H
#define BLOCKNETICONLABEL_H

#include <QPushButton>
#include <QLabel>
#include <QPixmap>
#include <QHBoxLayout>

class BlocknetIconLabel : public QPushButton
{
    Q_OBJECT
public:
    explicit BlocknetIconLabel(QPushButton *parent = nullptr);
    ~BlocknetIconLabel() override;
    void setIcon(const QString active, const QString disabled);
    void setLabel(const QString &label);

signals:

public slots:

protected:
    void paintEvent(QPaintEvent *e) override;

private slots:
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

#endif // BLOCKNETICONLABEL_H
