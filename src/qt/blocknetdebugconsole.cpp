// Copyright (c) 2011-2019 The Bitcoin Core developers
// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetdebugconsole.h>

#include <qt/blocknetguiutil.h>

#include <qt/guiutil.h>
#include <qt/peertablemodel.h>

#include <rpc/client.h>
#include <shutdown.h>
#include <util/strencodings.h>

#include <QApplication>
#include <QKeyEvent>
#include <QScrollBar>
#include <QSettings>
#include <QThread>
#include <QTime>

#include <boost/lexical_cast.hpp>

const struct {
    const char *url;
    const char *source;
} ICON_MAPPING[] = {
    {"cmd-request", ":/icons/tx_input"},
    {"cmd-reply", ":/icons/tx_output"},
    {"cmd-error", ":/icons/tx_output"},
    {"misc", ":/icons/tx_inout"},
    {nullptr, nullptr}
};

namespace BlocknetDebugConsoleRPC {

const int CONSOLE_HISTORY = 50;
const QSize FONT_RANGE(4, 40);
const char fontSizeSettingsKey[] = "consoleFontSize";

// don't add private key handling cmd's to the history
const QStringList historyFilter = QStringList()
        << "importprivkey"
        << "importmulti"
        << "sethdseed"
        << "signmessagewithprivkey"
        << "signrawtransaction"
        << "signrawtransactionwithkey"
        << "walletpassphrase"
        << "walletpassphrasechange"
        << "encryptwallet";

static QString categoryClass(int category)
    {
        switch(category)
        {
            case BlocknetDebugConsole::CMD_REQUEST:  return "cmd-request"; break;
            case BlocknetDebugConsole::CMD_REPLY:    return "cmd-reply"; break;
            case BlocknetDebugConsole::CMD_ERROR:    return "cmd-error"; break;
            default:                       return "misc";
        }
}

class QtRPCTimerInterface: public RPCTimerInterface
{
public:
    ~QtRPCTimerInterface() {}
    const char *Name() { return "Qt"; }
    RPCTimerBase* NewTimer(std::function<void()>& func, int64_t millis)
    {
        return new QtRPCTimerBase(func, millis);
    }
};

void RPCExecutor::request(const QString &command, const WalletModel* wallet_model)
{
    try
    {
        std::string result;
        std::string executableCommand = command.toStdString() + "\n";

        // Catch the console-only-help command before RPC call is executed and reply with help text as-if a RPC reply.
        if(executableCommand == "help-console\n") {
            Q_EMIT reply(BlocknetDebugConsole::CMD_REPLY, QString(("\n"
                "This console accepts RPC commands using the standard syntax.\n"
                "   example:    getblockhash 0\n\n"

                "This console can also accept RPC commands using the parenthesized syntax.\n"
                "   example:    getblockhash(0)\n\n"

                "Commands may be nested when specified with the parenthesized syntax.\n"
                "   example:    getblock(getblockhash(0) 1)\n\n"

                "A space or a comma can be used to delimit arguments for either syntax.\n"
                "   example:    getblockhash 0\n"
                "               getblockhash,0\n\n"

                "Named results can be queried with a non-quoted key string in brackets using the parenthesized syntax.\n"
                "   example:    getblock(getblockhash(0) 1)[tx]\n\n"

                "Results without keys can be queried with an integer in brackets using the parenthesized syntax.\n"
                "   example:    getblock(getblockhash(0),1)[tx][0]\n\n")));
            return;
        }
        if (!BlocknetDebugConsole::RPCExecuteCommandLine(m_node, result, executableCommand, nullptr, wallet_model)) {
            Q_EMIT reply(BlocknetDebugConsole::CMD_ERROR, QString("Parse error: unbalanced ' or \""));
            return;
        }

        Q_EMIT reply(BlocknetDebugConsole::CMD_REPLY, QString::fromStdString(result));
    }
    catch (UniValue& objError)
    {
        try // Nice formatting for standard-format error
        {
            int code = find_value(objError, "code").get_int();
            std::string message = find_value(objError, "message").get_str();
            Q_EMIT reply(BlocknetDebugConsole::CMD_ERROR, QString::fromStdString(message) + " (code " + QString::number(code) + ")");
        }
        catch (const std::runtime_error&) // raised when converting to invalid type, i.e. missing code or message
        {   // Show raw JSON object
            Q_EMIT reply(BlocknetDebugConsole::CMD_ERROR, QString::fromStdString(objError.write()));
        }
    }
    catch (const std::exception& e)
    {
        Q_EMIT reply(BlocknetDebugConsole::CMD_ERROR, QString("Error: ") + QString::fromStdString(e.what()));
    }
}

}

BlocknetDebugConsole::BlocknetDebugConsole(interfaces::Node& node, const PlatformStyle* platformStyle,
                                           QWidget *popup, int id, QFrame *parent)
                                                                : BlocknetToolsPage(id, parent),
                                                                  m_node(node),
                                                                  platformStyle(platformStyle),
                                                                  popupWidget(popup),
                                                                  layout(new QVBoxLayout)
{
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(layout);
    layout->setContentsMargins(0, 10, 0, 10);

    titleLbl = new QLabel(tr("Debug Console"));
    titleLbl->setObjectName("h2");

    messagesWidget = new QTextEdit;
    messagesWidget->setObjectName("consoleOutput");
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
    inputLbl->setObjectName("console");
    lineEdit = new QLineEdit;
    lineEdit->setObjectName("console");
    lineEdit->setMinimumHeight(BGU::spi(35));
    clearButton = new BlocknetLabelBtn;
    clearButton->setText(tr("Clear window"));

    auto fontBtnsBox = new QFrame;
    auto fontBtnsBoxLayout = new QHBoxLayout;
    fontBtnsBox->setLayout(fontBtnsBoxLayout);
    fontBtnsBoxLayout->setContentsMargins(QMargins());
    platformStyle->getImagesOnButtons();
    fontBiggerButton = new QPushButton;
    fontSmallerButton = new QPushButton;
    fontBiggerButton->setIcon(platformStyle->SingleColorIcon(":/icons/fontbigger"));
    fontSmallerButton->setIcon(platformStyle->SingleColorIcon(":/icons/fontsmaller"));
    fontBtnsBoxLayout->addStretch(1);
    fontBtnsBoxLayout->addWidget(fontSmallerButton, 0, Qt::AlignBottom);
    fontBtnsBoxLayout->addWidget(fontBiggerButton, 0, Qt::AlignBottom);

    consoleLayout->addWidget(inputLbl);
    consoleLayout->addWidget(lineEdit);
    consoleLayout->addWidget(clearButton);

    auto *titleBox = new QFrame;
    auto *titleBoxLayout = new QHBoxLayout;
    titleBox->setLayout(titleBoxLayout);
    titleBoxLayout->setContentsMargins(QMargins());
    titleBoxLayout->addWidget(titleLbl, 0, Qt::AlignTop | Qt::AlignLeft);
    titleBoxLayout->addWidget(fontBtnsBox);

    layout->addWidget(titleBox);
    layout->addSpacing(10);
    layout->addWidget(messagesWidget);
    layout->addSpacing(10);
    layout->addWidget(consoleBox);
    layout->addSpacing(40);

    // Install event filter for up and down arrow
    lineEdit->installEventFilter(this);
    messagesWidget->installEventFilter(this);

    // Register RPC timer interface
    rpcTimerInterface = new BlocknetDebugConsoleRPC::QtRPCTimerInterface();
    // avoid accidentally overwriting an existing, non QTThread
    // based timer interface
    m_node.rpcSetTimerInterfaceIfUnset(rpcTimerInterface);

    connect(clearButton, &BlocknetLabelBtn::clicked, this, &BlocknetDebugConsole::clear);
    connect(lineEdit, &QLineEdit::returnPressed, this, &BlocknetDebugConsole::on_lineEdit_returnPressed);
    connect(clearButton, &QPushButton::clicked, this, &BlocknetDebugConsole::clear);
    connect(fontBiggerButton, &QPushButton::clicked, this, &BlocknetDebugConsole::fontBigger);
    connect(fontSmallerButton, &QPushButton::clicked, this, &BlocknetDebugConsole::fontSmaller);

    QSettings settings;
    consoleFontSize = settings.value(BlocknetDebugConsoleRPC::fontSizeSettingsKey, QFontInfo(QFont()).pointSize()).toInt();
    clear();
}

BlocknetDebugConsole::~BlocknetDebugConsole() {
    thread.quit();
    thread.wait();
    m_node.rpcUnsetTimerInterface(rpcTimerInterface);
    delete rpcTimerInterface;
}

void BlocknetDebugConsole::setModels(ClientModel *c, WalletModel *w) {
    clientModel = c;
    walletModel = w;

    if (!clientModel || ShutdownRequested()) {
        // Client model is being set to 0, this means shutdown() is about to be called.
        thread.quit();
        thread.wait();
        return;
    }

    if (clientModel->getPeerTableModel() && clientModel->getBanTableModel()) {
        //Setup autocomplete and attach it
        QStringList wordList;
        std::vector<std::string> commandList = m_node.listRpcCommands();
        for (size_t i = 0; i < commandList.size(); ++i) {
            wordList << commandList[i].c_str();
            wordList << ("help " + commandList[i]).c_str();
        }

        wordList << "help-console";
        wordList.sort();
        autoCompleter = new QCompleter(wordList, this);
        autoCompleter->setModelSorting(QCompleter::CaseSensitivelySortedModel);
        lineEdit->setCompleter(autoCompleter);
        autoCompleter->popup()->installEventFilter(this);

        // Start thread to execute RPC commands.
        startExecutor();
    }
}

void BlocknetDebugConsole::focusInEvent(QFocusEvent *event) {
    QWidget::focusInEvent(event);
    lineEdit->setFocus();
}

/**
 * Split shell command line into a list of arguments and optionally execute the command(s).
 * Aims to emulate \c bash and friends.
 *
 * - Command nesting is possible with parenthesis; for example: validateaddress(getnewaddress())
 * - Arguments are delimited with whitespace or comma
 * - Extra whitespace at the beginning and end and between arguments will be ignored
 * - Text can be "double" or 'single' quoted
 * - The backslash \c \ is used as escape character
 *   - Outside quotes, any character can be escaped
 *   - Within double quotes, only escape \c " and backslashes before a \c " or another backslash
 *   - Within single quotes, no escaping is possible and no special interpretation takes place
 *
 * @param[in]    node    optional node to execute command on
 * @param[out]   strResult   stringified result from the executed command(chain)
 * @param[in]    strCommand  Command line to split
 * @param[in]    fExecute    set true if you want the command to be executed
 * @param[out]   pstrFilteredOut  Command line, filtered to remove any sensitive data
 */
bool BlocknetDebugConsole::RPCParseCommandLine(interfaces::Node* node, std::string &strResult, const std::string &strCommand, const bool fExecute, std::string * const pstrFilteredOut, const WalletModel* wallet_model)
{
    std::vector< std::vector<std::string> > stack;
    stack.push_back(std::vector<std::string>());

    enum CmdParseState
    {
        STATE_EATING_SPACES,
        STATE_EATING_SPACES_IN_ARG,
        STATE_EATING_SPACES_IN_BRACKETS,
        STATE_ARGUMENT,
        STATE_SINGLEQUOTED,
        STATE_DOUBLEQUOTED,
        STATE_ESCAPE_OUTER,
        STATE_ESCAPE_DOUBLEQUOTED,
        STATE_COMMAND_EXECUTED,
        STATE_COMMAND_EXECUTED_INNER
    } state = STATE_EATING_SPACES;
    std::string curarg;
    UniValue lastResult;
    unsigned nDepthInsideSensitive = 0;
    size_t filter_begin_pos = 0, chpos;
    std::vector<std::pair<size_t, size_t>> filter_ranges;

    auto add_to_current_stack = [&](const std::string& strArg) {
        if (stack.back().empty() && (!nDepthInsideSensitive) && BlocknetDebugConsoleRPC::historyFilter.contains(QString::fromStdString(strArg), Qt::CaseInsensitive)) {
            nDepthInsideSensitive = 1;
            filter_begin_pos = chpos;
        }
        // Make sure stack is not empty before adding something
        if (stack.empty()) {
            stack.push_back(std::vector<std::string>());
        }
        stack.back().push_back(strArg);
    };

    auto close_out_params = [&]() {
        if (nDepthInsideSensitive) {
            if (!--nDepthInsideSensitive) {
                assert(filter_begin_pos);
                filter_ranges.push_back(std::make_pair(filter_begin_pos, chpos));
                filter_begin_pos = 0;
            }
        }
        stack.pop_back();
    };

    std::string strCommandTerminated = strCommand;
    if (strCommandTerminated.back() != '\n')
        strCommandTerminated += "\n";
    for (chpos = 0; chpos < strCommandTerminated.size(); ++chpos)
    {
        char ch = strCommandTerminated[chpos];
        switch(state)
        {
            case STATE_COMMAND_EXECUTED_INNER:
            case STATE_COMMAND_EXECUTED:
            {
                bool breakParsing = true;
                switch(ch)
                {
                    case '[': curarg.clear(); state = STATE_COMMAND_EXECUTED_INNER; break;
                    default:
                        if (state == STATE_COMMAND_EXECUTED_INNER)
                        {
                            if (ch != ']')
                            {
                                // append char to the current argument (which is also used for the query command)
                                curarg += ch;
                                break;
                            }
                            if (curarg.size() && fExecute)
                            {
                                // if we have a value query, query arrays with index and objects with a string key
                                UniValue subelement;
                                if (lastResult.isArray())
                                {
                                    for(char argch: curarg)
                                        if (!IsDigit(argch))
                                            throw std::runtime_error("Invalid result query");
                                    subelement = lastResult[boost::lexical_cast<int>(curarg.c_str())];
                                }
                                else if (lastResult.isObject())
                                    subelement = find_value(lastResult, curarg);
                                else
                                    throw std::runtime_error("Invalid result query"); //no array or object: abort
                                lastResult = subelement;
                            }

                            state = STATE_COMMAND_EXECUTED;
                            break;
                        }
                        // don't break parsing when the char is required for the next argument
                        breakParsing = false;

                        // pop the stack and return the result to the current command arguments
                        close_out_params();

                        // don't stringify the json in case of a string to avoid doublequotes
                        if (lastResult.isStr())
                            curarg = lastResult.get_str();
                        else
                            curarg = lastResult.write(2);

                        // if we have a non empty result, use it as stack argument otherwise as general result
                        if (curarg.size())
                        {
                            if (stack.size())
                                add_to_current_stack(curarg);
                            else
                                strResult = curarg;
                        }
                        curarg.clear();
                        // assume eating space state
                        state = STATE_EATING_SPACES;
                }
                if (breakParsing)
                    break;
            }
            case STATE_ARGUMENT: // In or after argument
            case STATE_EATING_SPACES_IN_ARG:
            case STATE_EATING_SPACES_IN_BRACKETS:
            case STATE_EATING_SPACES: // Handle runs of whitespace
                switch(ch)
                {
                    case '"': state = STATE_DOUBLEQUOTED; break;
                    case '\'': state = STATE_SINGLEQUOTED; break;
                    case '\\': state = STATE_ESCAPE_OUTER; break;
                    case '(': case ')': case '\n':
                        if (state == STATE_EATING_SPACES_IN_ARG)
                            throw std::runtime_error("Invalid Syntax");
                        if (state == STATE_ARGUMENT)
                        {
                            if (ch == '(' && stack.size() && stack.back().size() > 0)
                            {
                                if (nDepthInsideSensitive) {
                                    ++nDepthInsideSensitive;
                                }
                                stack.push_back(std::vector<std::string>());
                            }

                            // don't allow commands after executed commands on baselevel
                            if (!stack.size())
                                throw std::runtime_error("Invalid Syntax");

                            add_to_current_stack(curarg);
                            curarg.clear();
                            state = STATE_EATING_SPACES_IN_BRACKETS;
                        }
                        if ((ch == ')' || ch == '\n') && stack.size() > 0)
                        {
                            if (fExecute) {
                                // Convert argument list to JSON objects in method-dependent way,
                                // and pass it along with the method name to the dispatcher.
                                UniValue params = RPCConvertValues(stack.back()[0], std::vector<std::string>(stack.back().begin() + 1, stack.back().end()));
                                std::string method = stack.back()[0];
                                std::string uri;
#ifdef ENABLE_WALLET
                                if (wallet_model) {
                                QByteArray encodedName = QUrl::toPercentEncoding(wallet_model->getWalletName());
                                uri = "/wallet/"+std::string(encodedName.constData(), encodedName.length());
                            }
#endif
                                assert(node);
                                lastResult = node->executeRpc(method, params, uri);
                            }

                            state = STATE_COMMAND_EXECUTED;
                            curarg.clear();
                        }
                        break;
                    case ' ': case ',': case '\t':
                        if(state == STATE_EATING_SPACES_IN_ARG && curarg.empty() && ch == ',')
                            throw std::runtime_error("Invalid Syntax");

                        else if(state == STATE_ARGUMENT) // Space ends argument
                        {
                            add_to_current_stack(curarg);
                            curarg.clear();
                        }
                        if ((state == STATE_EATING_SPACES_IN_BRACKETS || state == STATE_ARGUMENT) && ch == ',')
                        {
                            state = STATE_EATING_SPACES_IN_ARG;
                            break;
                        }
                        state = STATE_EATING_SPACES;
                        break;
                    default: curarg += ch; state = STATE_ARGUMENT;
                }
                break;
            case STATE_SINGLEQUOTED: // Single-quoted string
                switch(ch)
                {
                    case '\'': state = STATE_ARGUMENT; break;
                    default: curarg += ch;
                }
                break;
            case STATE_DOUBLEQUOTED: // Double-quoted string
                switch(ch)
                {
                    case '"': state = STATE_ARGUMENT; break;
                    case '\\': state = STATE_ESCAPE_DOUBLEQUOTED; break;
                    default: curarg += ch;
                }
                break;
            case STATE_ESCAPE_OUTER: // '\' outside quotes
                curarg += ch; state = STATE_ARGUMENT;
                break;
            case STATE_ESCAPE_DOUBLEQUOTED: // '\' in double-quoted text
                if(ch != '"' && ch != '\\') curarg += '\\'; // keep '\' for everything but the quote and '\' itself
                curarg += ch; state = STATE_DOUBLEQUOTED;
                break;
        }
    }
    if (pstrFilteredOut) {
        if (STATE_COMMAND_EXECUTED == state) {
            assert(!stack.empty());
            close_out_params();
        }
        *pstrFilteredOut = strCommand;
        for (auto i = filter_ranges.rbegin(); i != filter_ranges.rend(); ++i) {
            pstrFilteredOut->replace(i->first, i->second - i->first, "(…)");
        }
    }
    switch(state) // final state
    {
        case STATE_COMMAND_EXECUTED:
            if (lastResult.isStr())
                strResult = lastResult.get_str();
            else
                strResult = lastResult.write(2);
        case STATE_ARGUMENT:
        case STATE_EATING_SPACES:
            return true;
        default: // ERROR to end in one of the other states
            return false;
    }
}

bool BlocknetDebugConsole::eventFilter(QObject* obj, QEvent *event)
{
    if(event->type() == QEvent::KeyPress) // Special key handling
    {
        QKeyEvent *keyevt = static_cast<QKeyEvent*>(event);
        int key = keyevt->key();
        Qt::KeyboardModifiers mod = keyevt->modifiers();
        switch(key)
        {
            case Qt::Key_Up: if(obj == lineEdit) { browseHistory(-1); return true; } break;
            case Qt::Key_Down: if(obj == lineEdit) { browseHistory(1); return true; } break;
            case Qt::Key_PageUp: /* pass paging keys to messages widget */
            case Qt::Key_PageDown:
                if(obj == lineEdit)
                {
                    QApplication::postEvent(messagesWidget, new QKeyEvent(*keyevt));
                    return true;
                }
                break;
            case Qt::Key_Return:
            case Qt::Key_Enter:
                // forward these events to lineEdit
                if(obj == autoCompleter->popup()) {
                    QApplication::postEvent(lineEdit, new QKeyEvent(*keyevt));
                    autoCompleter->popup()->hide();
                    return true;
                }
                break;
            default:
                // Typing in messages widget brings focus to line edit, and redirects key there
                // Exclude most combinations and keys that emit no text, except paste shortcuts
                if(obj == messagesWidget && (
                        (!mod && !keyevt->text().isEmpty() && key != Qt::Key_Tab) ||
                        ((mod & Qt::ControlModifier) && key == Qt::Key_V) ||
                        ((mod & Qt::ShiftModifier) && key == Qt::Key_Insert)))
                {
                    lineEdit->setFocus();
                    QApplication::postEvent(lineEdit, new QKeyEvent(*keyevt));
                    return true;
                }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void BlocknetDebugConsole::fontBigger() {
    setFontSize(consoleFontSize+1);
}

void BlocknetDebugConsole::fontSmaller() {
    setFontSize(consoleFontSize-1);
}

void BlocknetDebugConsole::setFontSize(int newSize) {
    QSettings settings;

    //don't allow an insane font size
    if (newSize < BlocknetDebugConsoleRPC::FONT_RANGE.width() || newSize > BlocknetDebugConsoleRPC::FONT_RANGE.height())
        return;

    // temp. store the console content
    QString str = messagesWidget->toHtml();

    // replace font tags size in current content
    str.replace(QString("font-size:%1pt").arg(consoleFontSize), QString("font-size:%1pt").arg(newSize));

    // store the new font size
    consoleFontSize = newSize;
    settings.setValue(BlocknetDebugConsoleRPC::fontSizeSettingsKey, consoleFontSize);

    // clear console (reset icon sizes, default stylesheet) and re-add the content
    float oldPosFactor = 1.0 / messagesWidget->verticalScrollBar()->maximum() * messagesWidget->verticalScrollBar()->value();
    clear(false);
    messagesWidget->setHtml(str);
    messagesWidget->verticalScrollBar()->setValue(oldPosFactor * messagesWidget->verticalScrollBar()->maximum());
}

void BlocknetDebugConsole::clear(bool clearHistory) {
    messagesWidget->clear();
    if(clearHistory)
    {
        history.clear();
        historyPtr = 0;
    }
    lineEdit->clear();
    lineEdit->setFocus();

    // Add smoothly scaled icon images.
    // (when using width/height on an img, Qt uses nearest instead of linear interpolation)
    for(int i=0; ICON_MAPPING[i].url; ++i)
    {
        messagesWidget->document()->addResource(
                    QTextDocument::ImageResource,
                    QUrl(ICON_MAPPING[i].url),
                    platformStyle->SingleColorImage(ICON_MAPPING[i].source).scaled(QSize(consoleFontSize*2, consoleFontSize*2), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    }

    // Set default style sheet
    QFontInfo fixedFontInfo(GUIUtil::fixedPitchFont());
    messagesWidget->document()->setDefaultStyleSheet(
        QString(
                "table { }"
                "td.time { color: #6F8097; font-size: %2; padding-top: 3px; } "
                "td.message { font-family: %1; font-size: %2; white-space:pre-wrap; } "
                "td.cmd-request { color: #4BF5C6; } "
                "td.cmd-error { color: #FF7F71; } "
                ".secwarning { color: red; }"
                "b { color: #4BF5C6; } "
            ).arg(fixedFontInfo.family(), QString("%1pt").arg(consoleFontSize))
        );

#ifdef Q_OS_MAC
    QString clsKey = "(⌘)-L";
#else
    QString clsKey = "Ctrl-L";
#endif

    message(CMD_REPLY, (tr("Welcome to the %1 RPC console.").arg(tr(PACKAGE_NAME)) + "<br>" +
                        tr("Use up and down arrows to navigate history, and %1 to clear screen.").arg("<b>"+clsKey+"</b>") + "<br>" +
                        tr("Type %1 for an overview of available commands.").arg("<b>help</b>") + "<br>" +
                        tr("For more information on using this console type %1.").arg("<b>help-console</b>") +
                        "<br><span class=\"secwarning\"><br>" +
                        tr("WARNING: Scammers have been active, telling users to type commands here, stealing their wallet contents. Do not use this console without fully understanding the ramifications of a command.") +
                        "</span>"),
                        true);
}

void BlocknetDebugConsole::keyPressEvent(QKeyEvent *event) {
    if(windowType() != Qt::Widget && event->key() == Qt::Key_Escape)
        close();
}

void BlocknetDebugConsole::message(int category, const QString &message, bool html)
{
    QTime time = QTime::currentTime();
    QString timeString = time.toString();
    QString out;
    out += "<table><tr><td class=\"time\" width=\"65\">" + timeString + "</td>";
    out += "<td class=\"icon\" width=\"32\"><img src=\"" + BlocknetDebugConsoleRPC::categoryClass(category) + "\"></td>";
    out += "<td class=\"message " + BlocknetDebugConsoleRPC::categoryClass(category) + "\" valign=\"middle\">";
    if(html)
        out += message;
    else
        out += GUIUtil::HtmlEscape(message, false);
    out += "</td></tr></table>";
    messagesWidget->append(out);
}

void BlocknetDebugConsole::on_lineEdit_returnPressed()
{
    QString cmd = lineEdit->text();

    if(!cmd.isEmpty())
    {
        std::string strFilteredCmd;
        try {
            std::string dummy;
            if (!RPCParseCommandLine(nullptr, dummy, cmd.toStdString(), false, &strFilteredCmd)) {
                // Failed to parse command, so we cannot even filter it for the history
                throw std::runtime_error("Invalid command line");
            }
        } catch (const std::exception& e) {
            QMessageBox::critical(this, "Error", QString("Error: ") + QString::fromStdString(e.what()));
            return;
        }

        lineEdit->clear();

        cmdBeforeBrowsing = QString();

        WalletModel* wallet_model = walletModel;
#ifdef ENABLE_WALLET
        if (m_last_wallet_model != wallet_model) {
            if (wallet_model) {
                message(CMD_REQUEST, tr("Executing command using \"%1\" wallet").arg(wallet_model->getWalletName()));
            } else {
                message(CMD_REQUEST, tr("Executing command without any wallet"));
            }
            m_last_wallet_model = wallet_model;
        }
#endif

        message(CMD_REQUEST, QString::fromStdString(strFilteredCmd));
        Q_EMIT cmdRequest(cmd, m_last_wallet_model);

        cmd = QString::fromStdString(strFilteredCmd);

        // Remove command, if already in history
        history.removeOne(cmd);
        // Append command to history
        history.append(cmd);
        // Enforce maximum history size
        while(history.size() > BlocknetDebugConsoleRPC::CONSOLE_HISTORY)
            history.removeFirst();
        // Set pointer to end of history
        historyPtr = history.size();

        // Scroll console view to end
        scrollToEnd();
    }
}

void BlocknetDebugConsole::browseHistory(int offset) {
    // store current text when start browsing through the history
    if (historyPtr == history.size()) {
        cmdBeforeBrowsing = lineEdit->text();
    }

    historyPtr += offset;
    if(historyPtr < 0)
        historyPtr = 0;
    if(historyPtr > history.size())
        historyPtr = history.size();
    QString cmd;
    if(historyPtr < history.size())
        cmd = history.at(historyPtr);
    else if (!cmdBeforeBrowsing.isNull()) {
        cmd = cmdBeforeBrowsing;
    }
    lineEdit->setText(cmd);
}

void BlocknetDebugConsole::startExecutor() {
    auto *executor = new BlocknetDebugConsoleRPC::RPCExecutor(m_node);
    executor->moveToThread(&thread);

    // Replies from executor object must go to this object
    connect(executor, &BlocknetDebugConsoleRPC::RPCExecutor::reply, this, static_cast<void (BlocknetDebugConsole::*)(int, const QString&)>(&BlocknetDebugConsole::message));

    // Requests from this object must go to executor
    connect(this, &BlocknetDebugConsole::cmdRequest, executor, &BlocknetDebugConsoleRPC::RPCExecutor::request);

    // Make sure executor object is deleted in its own thread
    connect(&thread, &QThread::finished, executor, &BlocknetDebugConsoleRPC::RPCExecutor::deleteLater);

    // Default implementation of QThread::run() simply spins up an event loop in the thread,
    // which is what we want.
    thread.start();
}

void BlocknetDebugConsole::on_openDebugLogfileButton_clicked() {
    GUIUtil::openDebugLogfile();
}

void BlocknetDebugConsole::scrollToEnd() {
    QScrollBar *scrollbar = messagesWidget->verticalScrollBar();
    scrollbar->setValue(scrollbar->maximum());
}

void BlocknetDebugConsole::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
}
