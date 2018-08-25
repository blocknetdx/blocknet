// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETADDRESSEDITOR_H
#define BLOCKNETADDRESSEDITOR_H

#include <utility>
#include <functional>

#include <QTextEdit>
#include <QClipboard>
#include <QSet>
#include <QLabel>
#include <QStyleOption>
#include <QProxyStyle>
#include <QSize>

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

signals:
    void addresses();
    void returnPressed();

private slots:
    void onTextChanged();
    void onClipboard();
    void onSelectionChanged();

protected:
    void focusOutEvent(QFocusEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;

private:
    QClipboard *clipboard;
    QSet<QString> addrs;
    const int minHeight = 46;
    void cbOn(bool on = true);
    bool isValidAddress(QString &addr);
    bool equalS(QString s1, QString s2, Qt::CaseSensitivity sensitivity = Qt::CaseSensitive);
    QString prevText;
    bool backspacePressed;
    std::function<bool (QString&)> validator;
};

#endif // BLOCKNETADDRESSEDITOR_H