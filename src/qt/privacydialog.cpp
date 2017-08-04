// Copyright (c) 2011-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "privacydialog.h"
#include "ui_privacydialog.h"

#include "addressbookpage.h"
#include "addresstablemodel.h"
#include "bitcoinunits.h"
#include "guiutil.h"
#include "obfuscation.h"
#include "obfuscationconfig.h"
#include "optionsmodel.h"
#include "receiverequestdialog.h"
#include "recentrequeststablemodel.h"
#include "walletmodel.h"

#include <QAction>
#include <QClipboard>
#include <QCursor>
#include <QItemSelection>
#include <QMessageBox>
#include <QScrollBar>
#include <QSettings>
#include <QTextDocument>

PrivacyDialog::PrivacyDialog(QWidget* parent) : QDialog(parent),
                                                          ui(new Ui::PrivacyDialog),
                                                          walletModel(0),
                                                          currentBalance(-1)
{
    nDisplayUnit = 0; // just make sure it's not unitialized
    ui->setupUi(this);
    ui->labelzPIVSyncStatus->setText("(" + tr("out of sync") + ")");

//    if (fMasterNode) {
//        ui->pushButtonStartMixing->setText("(" + tr("Disabled") + ")");
//        ui->pushButtonRetryMixing->setText("(" + tr("Disabled") + ")");
//        ui->pushButtonResetMixing->setText("(" + tr("Disabled") + ")");
//    } else {
//        if (!fEnableObfuscation) {
//            ui->pushButtonStartMixing->setText(tr("Start"));
//        } else {
//            ui->pushButtonStartMixing->setText(tr("Stop"));
//        }
//        timer = new QTimer(this);
//        connect(timer, SIGNAL(timeout()), this, SLOT(obfuScationStatus()));
//        timer->start(1000);
//    }
    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);
}

PrivacyDialog::~PrivacyDialog()
{
    delete ui;
}

void PrivacyDialog::setModel(WalletModel* walletModel)
{
    this->walletModel = walletModel;

    if (walletModel && walletModel->getOptionsModel()) {
        // Keep up to date with wallet
        setBalance(walletModel->getBalance(), walletModel->getZerocoinBalance());
        connect(walletModel, SIGNAL(balanceChanged(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)), this, SLOT(setBalance(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)));
//        connect(ui->pushButtonRetryMixing, SIGNAL(clicked()), this, SLOT(obfuscationAuto()));
//        connect(ui->pushButtonResetMixing, SIGNAL(clicked()), this, SLOT(obfuscationReset()));
//        connect(ui->pushButtonStartMixing, SIGNAL(clicked()), this, SLOT(toggleObfuscation()));
    }
}

void PrivacyDialog::on_pasteButton_clicked()
{
    // Paste text from clipboard into recipient field
    ui->payTo->setText(QApplication::clipboard()->text());
}

void PrivacyDialog::on_addressBookButton_clicked()
{
    if (!walletModel)
        return;
    AddressBookPage dlg(AddressBookPage::ForSelection, AddressBookPage::SendingTab, this);
    dlg.setModel(walletModel->getAddressTableModel());
    if (dlg.exec()) {
        ui->payTo->setText(dlg.getReturnValue());
        ui->payAmount->setFocus();
    }
}

void PrivacyDialog::on_payTo_textChanged(const QString& address)
{
    updateLabel(address);
}

bool PrivacyDialog::updateLabel(const QString& address)
{
    if (!walletModel)
        return false;

    // Fill in label from address book, if address has an associated label
    QString associatedLabel = walletModel->getAddressTableModel()->labelForAddress(address);
    if (!associatedLabel.isEmpty()) {
        ui->addAsLabel->setText(associatedLabel);
        return true;
    }

    return false;
}

void PrivacyDialog::setBalance(const CAmount& balance, const CAmount& zeroCoinBalance)
{
    currentBalance = balance;
    currentZerocoinBalance = zeroCoinBalance;
}

void PrivacyDialog::updateDisplayUnit()
{
    if (walletModel && walletModel->getOptionsModel()) {
        nDisplayUnit = walletModel->getOptionsModel()->getDisplayUnit();
        if (currentBalance != -1)
            setBalance(currentBalance, currentZerocoinBalance);

//        // Update txdelegate->unit with the current unit
//        txdelegate->unit = nDisplayUnit;
//
//        ui->listTransactions->update();
    }
}

void PrivacyDialog::showOutOfSyncWarning(bool fShow)
{
    ui->labelzPIVSyncStatus->setVisible(fShow);
}
