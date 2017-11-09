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
