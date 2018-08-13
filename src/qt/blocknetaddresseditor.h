// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETADDRESSEDITOR_H
#define BLOCKNETADDRESSEDITOR_H

#include <QTextEdit>
#include <QClipboard>
#include <QSet>
#include <QLabel>
#include <QStyleOption>
#include <QProxyStyle>

class BlocknetAddressEditor : public QTextEdit
{
    Q_OBJECT
public:
    explicit BlocknetAddressEditor(int width = 675, QTextEdit *parent = nullptr);
    void setPlaceholderText(const QString &placeholderText);
    void addAddress(QString addr);
    QSet<QString> getAddresses() {
        return this->addrs;
    }

protected:

signals:
    void addresses();
    void returnPressed();

private slots:
    void onTextChanged();
    void onClipboard();
    void onSelectionChanged();

protected:
    void focusOutEvent(QFocusEvent *e) override;
    bool event(QEvent *e) override;

    void keyPressEvent(QKeyEvent *e) override;

private:
    QClipboard *clipboard;
    QSet<QString> addrs;
    const int minHeight = 46;
    void cbOn(bool on = true);
    bool isValidAddress(QString &addr);
    bool equalS(QString s1, QString s2, Qt::CaseSensitivity sensitivity = Qt::CaseSensitive);
    QLabel *placeholder = nullptr;
    void displayPlaceholder();
};

#endif // BLOCKNETADDRESSEDITOR_H