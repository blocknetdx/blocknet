//******************************************************************************
//******************************************************************************

#ifndef XBRIDGETRANSACTIONDIALOG_H
#define XBRIDGETRANSACTIONDIALOG_H

#include "xbridgetransactionsmodel.h"
#include "xbridgeaddressbookview.h"

// #include "../walletmodel.h"

#include <QDialog>
#include <QStringList>
#include <QStringListModel>

#include <boost/signals2.hpp>

class QLineEdit;
class QComboBox;
class QPushButton;
class QLabel;

//******************************************************************************
//******************************************************************************
class XBridgeTransactionDialog : public QDialog
{
    Q_OBJECT
public:
    explicit XBridgeTransactionDialog(XBridgeTransactionsModel & model, QWidget *parent = 0);
    ~XBridgeTransactionDialog();

public:
    // void setWalletModel(WalletModel * model);

    void setPendingId(const uint256 & id, const std::vector<unsigned char> & hubAddress);
    void setFromAmount(double amount);
    void setFromCurrency(const QString & currency);
    void setToAmount(double amount);
    void setToCurrency(const QString & currency);

signals:

private:
    void setupUI();

    void onWalletListReceived(const std::vector<string> & wallets);

private slots:
    void onWalletListReceivedHandler(const QStringList & wallets);
    void onSendTransaction();

    void onPasteFrom();
    void onAddressBookFrom();
    void onPasteTo();
    void onAddressBookTo();
    
    double accountBalance(const std::string & currency);

private:
    // WalletModel              * m_walletModel;

    XBridgeTransactionsModel & m_model;

    uint256                    m_pendingId;
    std::vector<unsigned char> m_hubAddress;

    QLineEdit    * m_addressFrom;
    QLineEdit    * m_addressTo;
    QLineEdit    * m_amountFrom;
    QLineEdit    * m_amountTo;
    QComboBox    * m_currencyFrom;
    QComboBox    * m_currencyTo;
    QLabel       * m_balanceFrom;
    QLabel       * m_balanceTo;
    QPushButton  * m_btnSend;

    XBridgeAddressBookView m_addressBook;

    QStringList      m_thisWallets;
    QStringListModel m_thisWalletsModel;

    QStringList      m_wallets;
    QStringListModel m_walletsModel;

    boost::signals2::connection m_walletsNotifier;
};

#endif // XBRIDGETRANSACTIONDIALOG_H
