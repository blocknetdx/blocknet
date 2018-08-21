// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETDROPDOWN_H
#define BLOCKNETDROPDOWN_H

#include <QComboBox>
#include <QVariant>

/* QComboBox that can be used with QDataWidgetMapper to select ordinal values from a model. */
class BlocknetDropdown : public QComboBox
{
    Q_OBJECT

    Q_PROPERTY(QVariant value READ value WRITE setValue NOTIFY valueChanged USER true)

public:
    explicit BlocknetDropdown(const QStringList &list, QWidget* parent = nullptr);

    QVariant value() const;
    void setValue(const QVariant& value);
    void showPopup();

signals:
    void valueChanged();

private slots:
    void handleSelectionChanged(int idx);

private:
    const int ddW = 180;
    const int ddH = 40;
};

#endif // BLOCKNETDROPDOWN_H
