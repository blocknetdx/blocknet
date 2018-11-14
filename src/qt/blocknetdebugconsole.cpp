// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetdebugconsole.h"

#include "guiutil.h"
#include "json/json_spirit_value.h"
#include "rpcclient.h"
#include "rpcserver.h"

#include <QDir>
#include <QTime>
#include <QThread>
#include <QScrollBar>
#include <QKeyEvent>
#include <QApplication>

const QSize ICON_SIZE(6, 6);
const int CONSOLE_HISTORY = 50;

const struct {
    const char* url;
    const char* source;
} ICON_MAPPING[] = {
    {"cmd-request", ":/icons/console-arrow-in"},
    {"cmd-reply", ":/icons/console-arrow-out"},
    {"cmd-error", ":/icons/console-arrow-out"},
    {"misc", ":/icons/console-arrow-out"},
    {NULL, NULL}};

static QString categoryClass(int category)
{
    switch (category) {
    case BlocknetDebugConsole::CMD_REQUEST:
        return "cmd-request";
        break;
    case BlocknetDebugConsole::CMD_REPLY:
        return "cmd-reply";
        break;
    case BlocknetDebugConsole::CMD_ERROR:
        return "cmd-error";
        break;
    default:
        return "misc";
    }
}

/* Object for executing console RPC commands in a separate thread.
*/
class RPCExecutor : public QObject
{
    Q_OBJECT

public slots:
    void request(const QString& command);

signals:
    void reply(int category, const QString& command);
};

#include "rpcconsole.moc"

/**
 * Split shell command line into a list of arguments. Aims to emulate \c bash and friends.
 *
 * - Arguments are delimited with whitespace
 * - Extra whitespace at the beginning and end and between arguments will be ignored
 * - Text can be "double" or 'single' quoted
 * - The backslash \c \ is used as escape character
 *   - Outside quotes, any character can be escaped
 *   - Within double quotes, only escape \c " and backslashes before a \c " or another backslash
 *   - Within single quotes, no escaping is possible and no special interpretation takes place
 *
 * @param[out]   args        Parsed arguments will be appended to this list
 * @param[in]    strCommand  Command line to split
 */
bool parseCommandLine(std::vector<std::string>& args, const std::string& strCommand)
{
    enum CmdParseState {
        STATE_EATING_SPACES,
        STATE_ARGUMENT,
        STATE_SINGLEQUOTED,
        STATE_DOUBLEQUOTED,
        STATE_ESCAPE_OUTER,
        STATE_ESCAPE_DOUBLEQUOTED
    } state = STATE_EATING_SPACES;
    std::string curarg;
    foreach (char ch, strCommand) {
        switch (state) {
        case STATE_ARGUMENT:      // In or after argument
        case STATE_EATING_SPACES: // Handle runs of whitespace
            switch (ch) {
            case '"':
                state = STATE_DOUBLEQUOTED;
                break;
            case '\'':
                state = STATE_SINGLEQUOTED;
                break;
            case '\\':
                state = STATE_ESCAPE_OUTER;
                break;
            case ' ':
            case '\n':
            case '\t':
                if (state == STATE_ARGUMENT) // Space ends argument
                {
                    args.push_back(curarg);
                    curarg.clear();
                }
                state = STATE_EATING_SPACES;
                break;
            default:
                curarg += ch;
                state = STATE_ARGUMENT;
            }
            break;
        case STATE_SINGLEQUOTED: // Single-quoted string
            switch (ch) {
            case '\'':
                state = STATE_ARGUMENT;
                break;
            default:
                curarg += ch;
            }
            break;
        case STATE_DOUBLEQUOTED: // Double-quoted string
            switch (ch) {
            case '"':
                state = STATE_ARGUMENT;
                break;
            case '\\':
                state = STATE_ESCAPE_DOUBLEQUOTED;
                break;
            default:
                curarg += ch;
            }
            break;
        case STATE_ESCAPE_OUTER: // '\' outside quotes
            curarg += ch;
            state = STATE_ARGUMENT;
            break;
        case STATE_ESCAPE_DOUBLEQUOTED:                  // '\' in double-quoted text
            if (ch != '"' && ch != '\\') curarg += '\\'; // keep '\' for everything but the quote and '\' itself
            curarg += ch;
            state = STATE_DOUBLEQUOTED;
            break;
        }
    }
    switch (state) // final state
    {
    case STATE_EATING_SPACES:
        return true;
    case STATE_ARGUMENT:
        args.push_back(curarg);
        return true;
    default: // ERROR to end in one of the other states
        return false;
    }
}

void RPCExecutor::request(const QString& command)
{
    std::vector<std::string> args;
    if (!parseCommandLine(args, command.toStdString())) {
        emit reply(BlocknetDebugConsole::CMD_ERROR, QString("Parse error: unbalanced ' or \""));
        return;
    }
    if (args.empty())
        return; // Nothing to do
    try {
        std::string strPrint;
        // Convert argument list to JSON objects in method-dependent way,
        // and pass it along with the method name to the dispatcher.
        json_spirit::Value result = tableRPC.execute(
            args[0],
            RPCConvertValues(args[0], std::vector<std::string>(args.begin() + 1,
                                                               args.end())));

        // Format result reply
        if (result.type() == json_spirit::null_type)
            strPrint = "";
        else if (result.type() == json_spirit::str_type)
            strPrint = result.get_str();
        else
            strPrint = write_string(result, true);

        emit reply(BlocknetDebugConsole::CMD_REPLY, QString::fromStdString(strPrint));
    } catch (json_spirit::Object& objError) {
        try // Nice formatting for standard-format error
        {
            int code = find_value(objError, "code").get_int();
            std::string message = find_value(objError, "message").get_str();
            emit reply(BlocknetDebugConsole::CMD_ERROR, QString::fromStdString(message) + " (code " + QString::number(code) + ")");
        } catch (std::runtime_error&) // raised when converting to invalid type, i.e. missing code or message
        {                             // Show raw JSON object
            emit reply(BlocknetDebugConsole::CMD_ERROR, QString::fromStdString(write_string(json_spirit::Value(objError), false)));
        }
    } catch (std::exception& e) {
        emit reply(BlocknetDebugConsole::CMD_ERROR, QString("Error: ") + QString::fromStdString(e.what()));
    }
}

BlocknetDebugConsole::BlocknetDebugConsole(QWidget *popup, int id, QFrame *parent) : BlocknetToolsPage(id, parent), popupWidget(popup), layout(new QVBoxLayout) {
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(layout);
    layout->setContentsMargins(0, 10, 0, 10);

    titleLbl = new QLabel(tr("Debug Console"));
    titleLbl->setObjectName("h2");

    messagesWidget = new QTextEdit;
    messagesWidget->setMinimumSize(0, 100);
    messagesWidget->setReadOnly(true);
    //messagesWidget->setTabKeyNavigation(false);
    //messagesWidget->setColumnCount(2);

    consoleBox = new QFrame;
    consoleBox->setObjectName("consoleBox");
    auto *consoleLayout = new QHBoxLayout;
    consoleLayout->setContentsMargins(QMargins());
    consoleLayout->setSpacing(5);
    consoleBox->setLayout(consoleLayout);

    inputLbl = new QLabel(tr(">"));
    lineEdit = new QLineEdit;
    clearButton = new BlocknetLabelBtn;
    clearButton->setText(tr("Clear message window"));

    consoleLayout->addWidget(inputLbl);
    consoleLayout->addWidget(lineEdit);
    consoleLayout->addWidget(clearButton);

    layout->addWidget(titleLbl, 0, Qt::AlignTop | Qt::AlignLeft);
    layout->addSpacing(10);
    layout->addWidget(messagesWidget);
    layout->addSpacing(10);
    layout->addWidget(consoleBox);
    layout->addSpacing(40);

    // Install event filter for up and down arrow
    lineEdit->installEventFilter(this);
    messagesWidget->installEventFilter(this);

    connect(clearButton, SIGNAL(clicked()), this, SLOT(clear()));
    connect(lineEdit, SIGNAL(returnPressed()), this, SLOT(on_lineEdit_returnPressed()));

    startExecutor();
}

void BlocknetDebugConsole::setWalletModel(WalletModel *w) {
    if (!walletModel)
        return;

    walletModel = w;
    clear();
}

void BlocknetDebugConsole::clear() {
    messagesWidget->clear();

    // Add smoothly scaled icon images.
    // (when using width/height on an img, Qt uses nearest instead of linear interpolation)
    for (int i = 0; ICON_MAPPING[i].url; ++i) {
        messagesWidget->document()->addResource(
            QTextDocument::ImageResource,
            QUrl(ICON_MAPPING[i].url),
            QImage(ICON_MAPPING[i].source).scaled(ICON_SIZE, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    }

    // Set default style sheet
    messagesWidget->document()->setDefaultStyleSheet(
        "table { }"
        "td.time { color: #6F8097; font-family: Roboto; font-size: 12px; padding-top: 3px; } "
        "td.icon { padding-top: 8px; } "
        "td.message { font-family: Roboto; font-size: 12px; color: white; } " // Todo: Remove fixed font-size
        "td.cmd-request { color: #4BF5C6; } "
        "td.cmd-error { color: red; } "
        "b { color: #4BF5C6; } ");

    message(CMD_REPLY, (tr("Welcome to the BlocknetDX RPC console.") + "<br>" +
                           tr("Use up and down arrows to navigate history, and <b>Ctrl-L</b> to clear screen.") + "<br>" +
                           tr("Type <b>help</b> for an overview of available commands.")),
        true);
}

void BlocknetDebugConsole::message(int category, const QString& message, bool html)
{
    QTime time = QTime::currentTime();
    QString timeString = time.toString();
    QString out;
    out += "<table><tr><td class=\"time\" width=\"65\">" + timeString + "</td>";
    out += "<td class=\"icon\" width=\"32\"><img src=\"" + categoryClass(category) + "\"></td>";
    out += "<td class=\"message " + categoryClass(category) + "\" valign=\"middle\">";
    if (html)
        out += message;
    else
        out += GUIUtil::HtmlEscape(message, true);
    out += "</td></tr></table>";
    messagesWidget->append(out);
}

void BlocknetDebugConsole::on_lineEdit_returnPressed()
{
    QString cmd = lineEdit->text();
    lineEdit->clear();

    if (!cmd.isEmpty()) {
        message(CMD_REQUEST, cmd);
        emit cmdRequest(cmd);
        // Remove command, if already in history
        history.removeOne(cmd);
        // Append command to history
        history.append(cmd);
        // Enforce maximum history size
        while (history.size() > CONSOLE_HISTORY)
            history.removeFirst();
        // Set pointer to end of history
        historyPtr = history.size();
        // Scroll console view to end
        scrollToEnd();
    }
}

void BlocknetDebugConsole::scrollToEnd()
{
    QScrollBar* scrollbar = messagesWidget->verticalScrollBar();
    scrollbar->setValue(scrollbar->maximum());
}

void BlocknetDebugConsole::startExecutor()
{
    QThread* thread = new QThread;
    RPCExecutor* executor = new RPCExecutor();
    executor->moveToThread(thread);

    // Replies from executor object must go to this object
    connect(executor, SIGNAL(reply(int, QString)), this, SLOT(message(int, QString)));
    // Requests from this object must go to executor
    connect(this, SIGNAL(cmdRequest(QString)), executor, SLOT(request(QString)));

    // On stopExecutor signal
    // - queue executor for deletion (in execution thread)
    // - quit the Qt event loop in the execution thread
    connect(this, SIGNAL(stopExecutor()), executor, SLOT(deleteLater()));
    connect(this, SIGNAL(stopExecutor()), thread, SLOT(quit()));
    // Queue the thread for deletion (in this thread) when it is finished
    connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));

    // Default implementation of QThread::run() simply spins up an event loop in the thread,
    // which is what we want.
    thread->start();
}

void BlocknetDebugConsole::browseHistory(int offset)
{
    historyPtr += offset;
    if (historyPtr < 0)
        historyPtr = 0;
    if (historyPtr > history.size())
        historyPtr = history.size();
    QString cmd;
    if (historyPtr < history.size())
        cmd = history.at(historyPtr);
    lineEdit->setText(cmd);
}

bool BlocknetDebugConsole::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::KeyPress) // Special key handling
    {
        QKeyEvent* keyevt = static_cast<QKeyEvent*>(event);
        int key = keyevt->key();
        Qt::KeyboardModifiers mod = keyevt->modifiers();
        switch (key) {
        case Qt::Key_Up:
            if (obj == lineEdit) {
                browseHistory(-1);
                return true;
            }
            break;
        case Qt::Key_Down:
            if (obj == lineEdit) {
                browseHistory(1);
                return true;
            }
            break;
        case Qt::Key_PageUp: /* pass paging keys to messages widget */
        case Qt::Key_PageDown:
            if (obj == lineEdit) {
                QApplication::postEvent(messagesWidget, new QKeyEvent(*keyevt));
                return true;
            }
            break;
        default:
            // Typing in messages widget brings focus to line edit, and redirects key there
            // Exclude most combinations and keys that emit no text, except paste shortcuts
            if (obj == messagesWidget && ((!mod && !keyevt->text().isEmpty() && key != Qt::Key_Tab) ||
                                                 ((mod & Qt::ControlModifier) && key == Qt::Key_V) ||
                                                 ((mod & Qt::ShiftModifier) && key == Qt::Key_Insert))) {
                lineEdit->setFocus();
                QApplication::postEvent(lineEdit, new QKeyEvent(*keyevt));
                return true;
            }
        }
    }
    return BlocknetToolsPage::eventFilter(obj, event);
}
