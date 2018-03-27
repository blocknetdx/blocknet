/// Copyright (c) 2011-2014 The Bitcoin developers
/// Copyright (c) 2014-2015 The Dash developers
/// Copyright (c) 2015-2017 The BlocknetDX developers
/// Distributed under the MIT/X11 software license, see the accompanying
/// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/// This module handles the events and callbacks from the WalletNonEncryptWarningDialog
/// form that is displayed upon program start if the user's wallet is currently un-encrypted.
/// This notice dialog may be disabled with a setting in the blocknextdx.conf file.
/// The flag is called -shownoencryptwarning and if set to 0 the notice dialog is disabled.
/// Default is 1 or true.


#ifndef WALLETNONENCRYPTWARNINGDIALOG_H
#define WALLETNONENCRYPTWARNINGDIALOG_H

#include <QDialog>

namespace Ui {
class WalletNonEncryptWarningDialog;
}

class WalletNonEncryptWarningDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WalletNonEncryptWarningDialog(QWidget *parent = 0);
    ~WalletNonEncryptWarningDialog();

private slots:
    void on_ignorePushButton_clicked();

    void on_encryptPushButton_clicked();

    void on_doNotShowAgainCheckBox_toggled(bool checked);

private:
    Ui::WalletNonEncryptWarningDialog *ui;
public:
    bool encryptWallet;
};

#endif // WALLETNONENCRYPTWARNINGDIALOG_H
