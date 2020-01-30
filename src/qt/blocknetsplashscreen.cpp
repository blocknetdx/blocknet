// Copyright (c) 2011-2018 The Bitcoin Core developers
// Copyright (c) 2019-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <qt/blocknetsplashscreen.h>
#include <qt/blocknetguiutil.h>

#include <qt/networkstyle.h>

#include <clientversion.h>
#include <interfaces/handler.h>
#include <interfaces/node.h>
#include <interfaces/wallet.h>
#include <ui_interface.h>
#include <util/system.h>
#include <version.h>

#include <QApplication>
#include <QCloseEvent>
#include <QDesktopWidget>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QRadialGradient>
#include <QSizePolicy>
#include <QVBoxLayout>


BlocknetSplashScreen::BlocknetSplashScreen(interfaces::Node& node, Qt::WindowFlags f, const NetworkStyle *networkStyle) :
    QWidget(nullptr, f), m_node(node)
{
    // define text to place
    QString titleText = tr(PACKAGE_NAME);
    QString versionText = QString(tr("Version %1")).arg(QString::fromStdString(FormatFullVersion()));
    QString copyrightTextBtc = QChar(0xA9) + QString(" 2009-2019 ") + QString(tr("The Bitcoin Core developers"));
    QString copyrightTextBlocknet = QChar(0xA9) + QString(" 2014-2020 ") + QString(tr("The Blocknet developers"));
    const QString &titleAddText = networkStyle->getTitleAddText();

    QString font = QApplication::font().toString();

    // load the bitmap for writing some text over it
    bg = new QLabel(this);
    auto pixmap = QPixmap(":/redesign/images/BlocknetSplash");
    QSize splashSize = QSize(BGU::spi(960), BGU::spi(640));
    pixmap.setDevicePixelRatio(BGU::dpr());
    pixmap = pixmap.scaled(static_cast<int>(splashSize.width()*pixmap.devicePixelRatio()),
            static_cast<int>(splashSize.height()*pixmap.devicePixelRatio()), Qt::KeepAspectRatio, Qt::SmoothTransformation);

    auto *layout = new QVBoxLayout;
    this->setLayout(layout);

    auto *versionLbl = new QLabel(versionText);
    auto *btcCopyLbl = new QLabel(copyrightTextBtc);
    auto *blockCopyLbl = new QLabel(copyrightTextBlocknet);
    auto *networkLbl = new QLabel(titleAddText);
    msg = new QLabel;
    msg->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto css = QString("font-size: 10pt; color: white; font-family: \"Roboto\"; background-color: transparent;");
    versionLbl->setStyleSheet(css);
    btcCopyLbl->setStyleSheet(css);
    blockCopyLbl->setStyleSheet(css);
    networkLbl->setStyleSheet(css + " font-weight: bold;");
    msg->setStyleSheet("font-size: 11pt; color: white; font-family: \"Roboto\"; background-color: transparent;");

    layout->addStretch(1);
    layout->addWidget(versionLbl);
    layout->addWidget(btcCopyLbl);
    layout->addWidget(blockCopyLbl);
    layout->addWidget(networkLbl, 0, Qt::AlignRight);
    layout->addWidget(msg, 0, Qt::AlignCenter);

    bg->setPixmap(pixmap);
    bg->setFixedSize(splashSize);

    // Set window title
    setWindowTitle(titleText + " " + titleAddText);

    // Resize window and move to center of desktop, disallow resizing
    QRect r(QPoint(), splashSize);
    resize(r.size());
    setFixedSize(r.size());
    move(QApplication::desktop()->screenGeometry().center() - r.center());

    subscribeToCoreSignals();
    installEventFilter(this);
}

BlocknetSplashScreen::~BlocknetSplashScreen()
{
    unsubscribeFromCoreSignals();
}

bool BlocknetSplashScreen::eventFilter(QObject * obj, QEvent * ev) {
    if (ev->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(ev);
        if(keyEvent->text()[0] == 'q') {
            m_node.startShutdown();
        }
    }
    return QObject::eventFilter(obj, ev);
}

void BlocknetSplashScreen::finish()
{
    /* If the window is minimized, hide() will be ignored. */
    /* Make sure we de-minimize the splashscreen window before hiding */
    if (isMinimized())
        showNormal();
    hide();
    deleteLater(); // No more need for this
}

static void InitMessage(BlocknetSplashScreen *splash, const std::string &message)
{
    QMetaObject::invokeMethod(splash, "showMessage", Qt::QueuedConnection,
            Q_ARG(QString, QString::fromStdString(message)));
}

static void ShowProgress(BlocknetSplashScreen *splash, const std::string &title, int nProgress, bool resume_possible)
{
    InitMessage(splash, title + strprintf(" %d", nProgress) + "%");
}
#ifdef ENABLE_WALLET
void BlocknetSplashScreen::ConnectWallet(std::unique_ptr<interfaces::Wallet> wallet)
{
    m_connected_wallet_handlers.emplace_back(wallet->handleShowProgress(std::bind(ShowProgress, this, std::placeholders::_1, std::placeholders::_2, false)));
    m_connected_wallets.emplace_back(std::move(wallet));
}
#endif

void BlocknetSplashScreen::subscribeToCoreSignals()
{
    // Connect signals to client
    m_handler_init_message = m_node.handleInitMessage(std::bind(InitMessage, this, std::placeholders::_1));
    m_handler_show_progress = m_node.handleShowProgress(std::bind(ShowProgress, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
#ifdef ENABLE_WALLET
    m_handler_load_wallet = m_node.handleLoadWallet([this](std::unique_ptr<interfaces::Wallet> wallet) { ConnectWallet(std::move(wallet)); });
#endif
}

void BlocknetSplashScreen::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    m_handler_init_message->disconnect();
    m_handler_show_progress->disconnect();
    for (const auto& handler : m_connected_wallet_handlers) {
        handler->disconnect();
    }
    m_connected_wallet_handlers.clear();
    m_connected_wallets.clear();
}

void BlocknetSplashScreen::showMessage(const QString &message)
{
    msg->setText(message);
}

void BlocknetSplashScreen::closeEvent(QCloseEvent *event)
{
    m_node.startShutdown(); // allows an "emergency" shutdown during startup
    event->ignore();
}
