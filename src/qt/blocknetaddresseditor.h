// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETADDRESSEDITOR_H
#define BLOCKNET_QT_BLOCKNETADDRESSEDITOR_H

#include <utility>
#include <functional>

#include <QClipboard>
#include <QLabel>
#include <QProxyStyle>
#include <QSet>
#include <QSize>
#include <QStyleOption>
#include <QTextEdit>

class BlocknetAddressEditor : public QTextEdit
{
    Q_OBJECT
public:
    explicit BlocknetAddressEditor(int width = 675, QTextEdit *parent = nullptr);
    void addAddress(QString addr);
    QSet<QString> getAddresses() {
        return this->addrs;
    }
    void clearData() {
        this->blockSignals(true);
        this->addrs.clear();
        prevText = QString();
        backspacePressed = false;
        this->clear();// resize
        this->setFixedHeight(this->optimalSize().height());
        this->blockSignals(false);
    }
    void setAddressValidator(std::function<bool (QString&)> validator) {
        this->validator = std::move(validator);
    }

protected:
    QSize optimalSize() const;

Q_SIGNALS:
    void addresses();
    void returnPressed();

private Q_SLOTS:
    void onTextChanged();
    void onClipboard();
    void onSelectionChanged();

protected:
    void focusOutEvent(QFocusEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;

private:
    QClipboard *clipboard;
    QSet<QString> addrs;
    const int minHeight;
    void cbOn(bool on = true);
    bool isValidAddress(QString &addr);
    bool equalS(QString s1, QString s2, Qt::CaseSensitivity sensitivity = Qt::CaseSensitive);
    QString prevText;
    bool backspacePressed;
    std::function<bool (QString&)> validator;
};

#endif // BLOCKNET_QT_BLOCKNETADDRESSEDITOR_H