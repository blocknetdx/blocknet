// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETDEBUGCONSOLE_H
#define BLOCKNET_QT_BLOCKNETDEBUGCONSOLE_H

#include <qt/blocknetlabelbtn.h>
#include <qt/blocknetpeerdetails.h>
#include <qt/blocknettoolspage.h>

#include <qt/platformstyle.h>

#include <interfaces/node.h>
#include <rpc/server.h>

#include <QCompleter>
#include <QFocusEvent>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QTimer>
#include <QThread>
#include <QVBoxLayout>

namespace BlocknetDebugConsoleRPC {

class RPCExecutor : public QObject
{
Q_OBJECT
public:
    explicit RPCExecutor(interfaces::Node& node) : m_node(node) {}

public Q_SLOTS:
    void request(const QString &command, const WalletModel* wallet_model);

Q_SIGNALS:
    void reply(int category, const QString &command);

private:
    interfaces::Node& m_node;
};

class QtRPCTimerBase: public QObject, public RPCTimerBase
{
Q_OBJECT
public:
    QtRPCTimerBase(std::function<void()>& _func, int64_t millis): func(_func) {
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, [this]{ func(); });
        timer.start(millis);
    }
    ~QtRPCTimerBase() {}
private:
    QTimer timer;
    std::function<void()> func;
};
}

class BlocknetDebugConsole : public BlocknetToolsPage {
    Q_OBJECT
public:
    explicit BlocknetDebugConsole(interfaces::Node& node, const PlatformStyle* platformStyle,
                                  QWidget *popup, int id, QFrame *parent = nullptr);
    ~BlocknetDebugConsole();

    static bool RPCParseCommandLine(interfaces::Node* node, std::string &strResult, const std::string &strCommand, bool fExecute, std::string * const pstrFilteredOut = nullptr, const WalletModel* wallet_model = nullptr);
    static bool RPCExecuteCommandLine(interfaces::Node& node, std::string &strResult, const std::string &strCommand, std::string * const pstrFilteredOut = nullptr, const WalletModel* wallet_model = nullptr) {
        return RPCParseCommandLine(&node, strResult, strCommand, true, pstrFilteredOut, wallet_model);
    }
    void setModels(ClientModel *c, WalletModel *w);

    enum MessageClass {
        MC_ERROR,
        MC_DEBUG,
        CMD_REQUEST,
        CMD_REPLY,
        CMD_ERROR
    };

public Q_SLOTS:
    void clear(bool clearHistory = true);
    void fontBigger();
    void fontSmaller();
    void setFontSize(int newSize);
    /** Append the message to the message widget */
    void message(int category, const QString &msg) { message(category, msg, false); }
    void message(int category, const QString &message, bool html);
    /** Go forward or back in history */
    void browseHistory(int offset);
    /** Scroll console view to end */
    void scrollToEnd();

protected:
    virtual bool eventFilter(QObject* obj, QEvent *event);
    void keyPressEvent(QKeyEvent *);

private Q_SLOTS:
    void on_lineEdit_returnPressed();
    /** open the debug.log from the current datadir */
    void on_openDebugLogfileButton_clicked();
    void focusInEvent(QFocusEvent *event);
    void resizeEvent(QResizeEvent *event);

Q_SIGNALS:
    // For RPC command executor
    void cmdRequest(const QString & command, const WalletModel * wallet_model);

private:
    void startExecutor();

private:
    interfaces::Node& m_node;
    ClientModel *clientModel;
    WalletModel *walletModel;
    QVBoxLayout *layout;
    QLabel *titleLbl;
    QWidget *popupWidget;
    QTextEdit *messagesWidget;
    QFrame *consoleBox;
    QLabel *inputLbl;
    QLineEdit *lineEdit;
    BlocknetLabelBtn *clearButton;

    const PlatformStyle *platformStyle;
    QPushButton *fontBiggerButton;
    QPushButton *fontSmallerButton;
    QStringList history;
    int historyPtr = 0;
    QString cmdBeforeBrowsing;
    RPCTimerInterface *rpcTimerInterface = nullptr;
    int consoleFontSize = 0;
    QCompleter *autoCompleter = nullptr;
    QThread thread;
    WalletModel *m_last_wallet_model{nullptr};
};

#endif // BLOCKNET_QT_BLOCKNETDEBUGCONSOLE_H
