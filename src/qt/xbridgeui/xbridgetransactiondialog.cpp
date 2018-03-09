//******************************************************************************
//******************************************************************************

#include "xbridgetransactiondialog.h"
// #include "../ui_interface.h"
// #include "../xbridgeconnector.h"
#include "xbridge/util/xutil.h"
#include "xbridge/xbridgeexchange.h"
#include "xbridge/xbridgeapp.h"
#include "xbridge/xbridgewalletconnector.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>

const QString testFrom("<input from address>");
const QString testTo("<input to address>");
// const QString testFromCurrency("XC");
// const QString testToCurrency("SWIFT");
const QString testFromAmount("0.0005");
const QString testToAmount("0.0005");

//******************************************************************************
//******************************************************************************
XBridgeTransactionDialog::XBridgeTransactionDialog(XBridgeTransactionsModel & model,
                                                   QWidget *parent)
    : QDialog(parent, Qt::Dialog)
    // , m_walletModel(0)
    , m_model(model)
    , m_addressBook(this)
{
    setupUI();

    xbridge::App & xapp = xbridge::App::instance();
    std::vector<std::string> wallets = xapp.availableCurrencies();
    for (const std::string & s : wallets)
    {
        m_thisWallets << QString::fromStdString(s);
    }
    m_thisWalletsModel.setStringList(m_thisWallets);

    m_currencyFrom->setCurrentIndex(0);

    onWalletListReceived(wallets);
}

//******************************************************************************
//******************************************************************************
XBridgeTransactionDialog::~XBridgeTransactionDialog()
{
    m_walletsNotifier.disconnect();
}

//******************************************************************************
//******************************************************************************
//void XBridgeTransactionDialog::setWalletModel(WalletModel * model)
//{
//    m_walletModel = model;
//}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionDialog::setPendingId(const uint256 & id,
                                            const std::vector<unsigned char> & hubAddress)
{
    m_pendingId  = id;
    m_hubAddress = hubAddress;

    bool isPending = m_pendingId != uint256();

    m_amountFrom->setEnabled(!isPending);
    m_amountTo->setEnabled(!isPending);
    m_currencyFrom->setEnabled(!isPending);
    m_currencyTo->setEnabled(!isPending);
}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionDialog::setFromAmount(double amount)
{
    m_amountFrom->setText(QString::number(amount));
}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionDialog::setToAmount(double amount)
{
    m_amountTo->setText(QString::number(amount));
}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionDialog::setFromCurrency(const QString & currency)
{
    int idx = m_wallets.indexOf(currency);
    m_currencyFrom->setCurrentIndex(idx);
}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionDialog::setToCurrency(const QString & currency)
{
    int idx = m_wallets.indexOf(currency);
    m_currencyTo->setCurrentIndex(idx);
}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionDialog::setupUI()
{
//    QRect r = geometry();
//    r.setWidth(qApp->activeWindow()->width());
//    setGeometry(r);

    QGridLayout * grid = new QGridLayout;

    QLabel * l = new QLabel(this);//QString::fromStdString(xbridge().xbridgeAddressAndPort()), this);
    grid->addWidget(l, 0, 0, 1, 5);

    l = new QLabel(trUtf8("from"), this);
    grid->addWidget(l, 1, 0, 1, 1);

    l = new QLabel(trUtf8("to"), this);
    grid->addWidget(l, 1, 4, 1, 1, Qt::AlignRight);

    m_addressFrom = new QLineEdit(this);
    m_addressFrom->setText(testFrom);
    m_addressFrom->setMinimumWidth(300);
    m_addressFrom->setReadOnly(true);
    m_addressFrom->setToolTip(trUtf8("Input source address"));
    // grid->addWidget(m_addressFrom, 2, 0, 1, 1);

    QPushButton * pasteFrom = new QPushButton(this);
    pasteFrom->setIcon(QIcon(":/icons/editpaste-white"));
    pasteFrom->setToolTip(trUtf8("Paste source address"));
    connect(pasteFrom, SIGNAL(clicked()), this, SLOT(onPasteFrom()));

    QPushButton * abFrom = new QPushButton(this);
    abFrom->setIcon(QIcon(":/icons/address-book-white"));
    abFrom->setToolTip(trUtf8("Show address book"));
    connect(abFrom, SIGNAL(clicked()), this, SLOT(onAddressBookFrom()));

    QHBoxLayout * hbox = new QHBoxLayout;
    hbox->addWidget(m_addressFrom);
    hbox->addWidget(pasteFrom);
    hbox->addWidget(abFrom);

    grid->addLayout(hbox, 2, 0, 1, 2);

    m_addressTo = new QLineEdit(this);
    m_addressTo->setText(testTo);
    m_addressTo->setMinimumWidth(300);
    m_addressTo->setReadOnly(true);
    m_addressTo->setToolTip(trUtf8("Input destination address"));
    // grid->addWidget(m_addressTo, 2, 3, 1, 1);

    QPushButton * pasteTo = new QPushButton(this);
    pasteTo->setIcon(QIcon(":/icons/editpaste-white"));
    pasteTo->setToolTip(trUtf8("Paste destination address"));
    connect(pasteTo, SIGNAL(clicked()), this, SLOT(onPasteTo()));

    QPushButton * abTo = new QPushButton(this);
    abTo->setIcon(QIcon(":/icons/address-book-white"));
    abTo->setToolTip(trUtf8("Show address book"));
    connect(abTo, SIGNAL(clicked()), this, SLOT(onAddressBookTo()));

    hbox = new QHBoxLayout;
    hbox->addWidget(m_addressTo);
    hbox->addWidget(pasteTo);
    hbox->addWidget(abTo);

    grid->addLayout(hbox, 2, 3, 1, 2);

    m_amountFrom = new QLineEdit(this);
    m_amountFrom->setText(testFromAmount);
    m_amountFrom->setToolTip(trUtf8("Source amount"));
    grid->addWidget(m_amountFrom, 3, 0, 1, 1);

    m_currencyFrom = new QComboBox(this);
    m_currencyFrom->setModel(&m_thisWalletsModel);
    m_currencyFrom->setFixedWidth(72);
    m_currencyFrom->setToolTip(trUtf8("Source currency"));
    grid->addWidget(m_currencyFrom, 3, 1, 1, 1);

    m_amountTo = new QLineEdit(this);
    m_amountTo->setText(testToAmount);
    m_amountTo->setToolTip(trUtf8("Destination amount"));
    grid->addWidget(m_amountTo, 3, 3, 1, 1);

    m_currencyTo = new QComboBox(this);
    m_currencyTo->setModel(&m_walletsModel);
    m_currencyTo->setFixedWidth(72);
    m_currencyTo->setToolTip(trUtf8("Destination currency"));
    grid->addWidget(m_currencyTo, 3, 4, 1, 1);

    l = new QLabel(trUtf8(" --- >>> "), this);
    grid->addWidget(l, 1, 2, 3, 1, Qt::AlignHCenter | Qt::AlignCenter);

    m_balanceFrom = new QLabel(trUtf8("Account balance:"));
    m_balanceTo = new QLabel(trUtf8("Account balance:"));

    grid->addWidget(m_balanceFrom, 4, 0, 1, 1);
    grid->addWidget(m_balanceTo, 4, 3, 1, 1);

    m_btnSend = new QPushButton(trUtf8("New Transaction"), this);
    m_btnSend->setEnabled(false);

    QPushButton * cancel = new QPushButton(trUtf8("Cancel"), this);

    hbox = new QHBoxLayout;
    hbox->addStretch();
    hbox->addWidget(m_btnSend);
    hbox->addWidget(cancel);

    grid->addLayout(hbox, 5, 0, 1, 5);

    QVBoxLayout * vbox = new QVBoxLayout;
    vbox->addLayout(grid);
    vbox->addStretch();

    setLayout(vbox);

    connect(m_btnSend, SIGNAL(clicked()), this, SLOT(onSendTransaction()));
    connect(cancel,    SIGNAL(clicked()), this, SLOT(reject()));
}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionDialog::onWalletListReceived(const std::vector<std::string> & wallets)
{
    QStringList list;
    for (const std::string & w : wallets)
    {
        list.push_back(QString::fromStdString(w));
    }

    QMetaObject::invokeMethod(this, "onWalletListReceivedHandler", Qt::QueuedConnection,
                              Q_ARG(QStringList, list));
}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionDialog::onWalletListReceivedHandler(const QStringList & wallets)
{
    QString w = m_currencyTo->currentText();

    m_wallets = wallets;
    m_walletsModel.setStringList(m_wallets);

    int idx = m_wallets.indexOf(w);
    m_currencyTo->setCurrentIndex(idx);

    m_btnSend->setEnabled(true);
}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionDialog::onSendTransaction()
{
//    std::string f = util::base64_decode(m_addressFrom->text().toStdString());
//    std::vector<unsigned char> from(f.begin(), f.end());
//    std::string t = util::base64_decode(m_addressTo->text().toStdString().c_str());
//    std::vector<unsigned char> to(t.begin(), t.end());
//    if (from.size() != 20 || to.size() != 20)
//    {
//        QMessageBox::warning(this, trUtf8("check parameters"), trUtf8("Invalid address"));
//        return;
//    }

    std::string from = m_addressFrom->text().toStdString();
    std::string to   = m_addressTo->text().toStdString().c_str();
    if (from.size() < 32 || from.size() > 36)
    {
        m_addressFrom->setFocus();
        QMessageBox::warning(this, trUtf8("check parameters"), trUtf8("Invalid from address"));
        return;
    }

    if (to.size() < 32 || to.size() > 36)
    {
        m_addressTo->setFocus();
        QMessageBox::warning(this, trUtf8("check parameters"), trUtf8("Invalid to address"));
        return;
    }

    std::string fromCurrency        = m_currencyFrom->currentText().toStdString();
    std::string toCurrency          = m_currencyTo->currentText().toStdString();
    if (fromCurrency.size() == 0 || toCurrency.size() == 0)
    {
        QMessageBox::warning(this, trUtf8("check parameters"), trUtf8("Invalid currency"));
        return;
    }

    double fromAmount      = m_amountFrom->text().toDouble();
    double toAmount        = m_amountTo->text().toDouble();
    double minimumAmount   = 1.0 / boost::numeric_cast<double>(xbridge::TransactionDescr::COIN);

    if (fromAmount < minimumAmount || toAmount < minimumAmount)
    {
        QMessageBox::warning(this, trUtf8("check parameters"), trUtf8("Invalid amount"));
        return;
    }

    if (m_pendingId != uint256())
    {
        // accept pending tx

        const auto error = m_model.newTransactionFromPending(m_pendingId, m_hubAddress, from, to);
        if(error != xbridge::SUCCESS)
        {
            QMessageBox::warning(this, trUtf8("check parameters"),
                                 trUtf8("Invalid address %1")
                                 .arg(xbridge::xbridgeErrorText(error, from).c_str()));
            return;
        }
    }
    else
    {
        // new tx
        const auto error = m_model.newTransaction(from, to, fromCurrency, toCurrency, fromAmount, toAmount);
        if (error != xbridge::SUCCESS)
        {
            QMessageBox::warning(this, trUtf8("check parameters"),

                                 trUtf8("Invalid amount %1 %2")
                                 .arg(xbridge::xbridgeErrorText(error).c_str())
                                 .arg(fromAmount));
            return;
        }
    }

    accept();
}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionDialog::onPasteFrom()
{
    m_addressFrom->setText(QApplication::clipboard()->text());
}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionDialog::onAddressBookFrom()
{
//    if (!m_walletModel)
//    {
//        return;
//    }

//    AddressBookPage dlg(AddressBookPage::ForSending, AddressBookPage::ReceivingTab, this);
//    dlg.setModel(m_walletModel->getAddressTableModel());
//    if (dlg.exec())
//    {
//        std::vector<unsigned char> tmp;
//        DecodeBase58Check(dlg.getReturnValue().toStdString(), tmp);
//        if (tmp.empty())
//        {
//            QMessageBox::warning(this, "", trUtf8("Error decode address"));
//            return;
//        }

//        m_addressFrom->setText(QString::fromStdString(EncodeBase64(&tmp[1], tmp.size()-1)));
//        m_addressFrom->setFocus();
//    }
    if (m_addressBook.exec() == QDialog::Accepted)
    {
        QString currency = QString::fromStdString(m_addressBook.selectedCurrency());
        setFromCurrency(currency);

        QString address = QString::fromStdString(m_addressBook.selectedAddress());
        m_addressFrom->setText(address);
        m_addressFrom->setFocus();

        QString balance = QString::number(accountBalance(m_addressBook.selectedCurrency()), 'g', 10);
        m_balanceFrom->setText(QString("Account balance: %1 %2").
                               arg(balance).
                               arg(currency));
    }
}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionDialog::onPasteTo()
{
    m_addressTo->setText(QApplication::clipboard()->text());
}

//******************************************************************************
//******************************************************************************
void XBridgeTransactionDialog::onAddressBookTo()
{
    if (m_addressBook.exec() == QDialog::Accepted)
    {
        QString currency = QString::fromStdString(m_addressBook.selectedCurrency());
        setToCurrency(currency);

        QString address  = QString::fromStdString(m_addressBook.selectedAddress());
        m_addressTo->setText(address);
        m_addressTo->setFocus();

        QString balance = QString::number(accountBalance(m_addressBook.selectedCurrency()), 'g', 10);
        m_balanceTo->setText(QString("Account balance: %1 %2").
                             arg(balance).
                             arg(currency));
    }
}

//******************************************************************************
//******************************************************************************
double XBridgeTransactionDialog::accountBalance(const std::string & currency)
{
    xbridge::WalletConnectorPtr conn = xbridge::App::instance().connectorByCurrency(currency);
    if (conn)
    {
        return conn->getWalletBalance();
    }
    return 0;
}
