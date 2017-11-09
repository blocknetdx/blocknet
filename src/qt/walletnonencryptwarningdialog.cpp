#include "qt/walletnonencryptwarningdialog.h"
#include "ui_walletnonencryptwarningdialog.h"

#include "guiutil.h"


WalletNonEncryptWarningDialog::WalletNonEncryptWarningDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::WalletNonEncryptWarningDialog)
{
    ui->setupUi(this);
    encryptWallet = false;
}

WalletNonEncryptWarningDialog::~WalletNonEncryptWarningDialog()
{
    delete ui;
}

void WalletNonEncryptWarningDialog::on_ignorePushButton_clicked()
{
    //encryptWallet = false;
    QDialog::done(false);
    //QDialog::close();
}

void WalletNonEncryptWarningDialog::on_encryptPushButton_clicked()
{
    //encryptWallet = true;
    QDialog::done(true);
}

void WalletNonEncryptWarningDialog::on_doNotShowAgainCheckBox_toggled(bool checked)
{

}
