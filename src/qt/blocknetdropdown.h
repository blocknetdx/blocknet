// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETDROPDOWN_H
#define BLOCKNET_QT_BLOCKNETDROPDOWN_H

#include <QComboBox>
#include <QVariant>
#include <QWheelEvent>

/* QComboBox that can be used with QDataWidgetMapper to select ordinal values from a model. */
class BlocknetDropdown : public QComboBox
{
    Q_OBJECT

    Q_PROPERTY(QVariant value READ value WRITE setValue NOTIFY valueChanged USER true)

public:
    explicit BlocknetDropdown(QWidget* parent = nullptr);
    explicit BlocknetDropdown(const QStringList &list, QWidget* parent = nullptr);

    QVariant value() const;
    void setValue(const QVariant& value);
    void showPopup() override;

protected:
    void wheelEvent(QWheelEvent *e) override;

Q_SIGNALS:
    void valueChanged();

private Q_SLOTS:
    void handleSelectionChanged(int idx);

private:
    const int ddW;
    const int ddH;
};

#endif // BLOCKNET_QT_BLOCKNETDROPDOWN_H
