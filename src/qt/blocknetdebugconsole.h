// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETDEBUGCONSOLE_H
#define BLOCKNETDEBUGCONSOLE_H

#include "blocknettools.h"
#include "blocknetpeerdetails.h"
#include "blocknetlabelbtn.h"

#include <QFrame>
#include <QVBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QLineEdit>

class BlocknetDebugConsole : public BlocknetToolsPage {
    Q_OBJECT

protected:
    virtual bool eventFilter(QObject* obj, QEvent* event);

public:
    explicit BlocknetDebugConsole(QWidget *popup, int id, QFrame *parent = nullptr);
    void setWalletModel(WalletModel *w);

    enum MessageClass {
        MC_ERROR,
        MC_DEBUG,
        CMD_REQUEST,
        CMD_REPLY,
        CMD_ERROR
    };

public slots:
    void clear();
    void message(int category, const QString& message, bool html = false);
    /** Scroll console view to end */
    void scrollToEnd();
    /** Go forward or back in history */
    void browseHistory(int offset);

private slots:
    void on_lineEdit_returnPressed();

signals:
    // For RPC command executor
    void stopExecutor();
    void cmdRequest(const QString& command);

private:
    QVBoxLayout *layout;
    QLabel *titleLbl;
    QWidget *popupWidget;
    QTextEdit *messagesWidget;
    QFrame *consoleBox;
    QLabel *inputLbl;
    QLineEdit *lineEdit;
    BlocknetLabelBtn *clearButton;
    QStringList history;
    int historyPtr;

    void startExecutor();
};

#endif // BLOCKNETDEBUGCONSOLE_H
