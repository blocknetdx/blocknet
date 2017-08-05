// Copyright (c) 2011-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "privacydialog.h"
#include "ui_privacydialog.h"

#include "addressbookpage.h"
#include "addresstablemodel.h"
#include "bitcoinunits.h"
#include "libzerocoin/Denominations.h"
#include "optionsmodel.h"
#include "walletmodel.h"

#include <QClipboard>

PrivacyDialog::PrivacyDialog(QWidget* parent) : QDialog(parent),
                                                          ui(new Ui::PrivacyDialog),
                                                          walletModel(0),
                                                          currentBalance(-1)
{
    nDisplayUnit = 0; // just make sure it's not unitialized
    ui->setupUi(this);
    ui->labelzPIVSyncStatus->setText("(" + tr("out of sync") + ")");
    ui->zPIVpayAmount->setValidator( new QIntValidator(0, 999999, this) ); // "Spending 999999 zPIV ought to be enough for anybody." - Bill Gates, 2017
    ui->labelMintAmountValue->setValidator( new QIntValidator(0, 999999, this) );
    ui->labelMintStatus->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    ui->labelMintStatus->setLineWidth (2);
    ui->labelMintStatus->setMidLineWidth (2);
    
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
        setBalance(walletModel->getBalance(),         walletModel->getUnconfirmedBalance(), walletModel->getImmatureBalance(), 
                   walletModel->getZerocoinBalance(), walletModel->getWatchBalance(),       walletModel->getWatchUnconfirmedBalance(), 
                   walletModel->getWatchImmatureBalance());
        
        connect(walletModel, SIGNAL(balanceChanged(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)), this, 
                               SLOT(setBalance(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)));
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
        ui->zPIVpayAmount->setFocus();
    }
}

void PrivacyDialog::on_pushButtonMintzPIV_clicked()
{
    if (!walletModel || !walletModel->getOptionsModel())
        return;

    // Reset message text
    ui->labelMintStatus->setText(tr("Mint Status: okay"));
    
    // Wallet must be unlocked for minting
    if (pwalletMain->IsLocked()){
        ui->labelMintStatus->setText(tr("Error: your wallet is locked. Please enter the wallet passphrase first."));
        return;
    }

    QString sAmount = ui->labelMintAmountValue->text();
    CAmount nAmount = sAmount.toInt() * COIN;

    // Minting amount must be > 0
    if(nAmount <= 0){
        ui->labelMintStatus->setText(tr("Message: Enter an amount > 0."));
        return;
    }

    int64_t nTime = GetTimeMillis();
    
    CWalletTx wtx;
    vector<CZerocoinMint> vMints;
    string strError = pwalletMain->MintZerocoin(nAmount, wtx, vMints);
    
    // Return if something went wrong during minting
    if (strError != ""){
        QString strErrorMessage = tr("Error: ") + QString::fromStdString(strError);
        ui->labelMintStatus->setText(strErrorMessage);
        return;
    }

    int64_t nDuration = GetTimeMillis() - nTime;
    
    // Minting successfully finished. Show some stats for entertainment.
    QString strStatsHeader = tr("Successful minted ") + ui->labelMintAmountValue->text() + tr(" zPIV in ") + 
                             QString::number(nDuration) + tr(" ms. Used denominations:\n");
    QString strStats = "";
    ui->labelMintStatus->setText(strStatsHeader);

    for (CZerocoinMint mint : vMints) {
        strStats = strStats + QString::number(mint.GetDenomination()) + " ";
        ui->labelMintStatus->setText(strStatsHeader + strStats);
    }

    // Available balance isn't always updated, so force it.
    setBalance(walletModel->getBalance(),         walletModel->getUnconfirmedBalance(), walletModel->getImmatureBalance(), 
               walletModel->getZerocoinBalance(), walletModel->getWatchBalance(),       walletModel->getWatchUnconfirmedBalance(), 
               walletModel->getWatchImmatureBalance());
    return;
}

void PrivacyDialog::on_pushButtonMintReset_clicked()
{
    if (!walletModel || !walletModel->getOptionsModel())
        return;

    QMessageBox::warning(this, tr("Reset Zerocoin"),
                tr("Test for Reset"),
                QMessageBox::Ok, QMessageBox::Ok);
            return;

        return;
}

void PrivacyDialog::on_pushButtonSpendzPIV_clicked()
{
    if (!walletModel || !walletModel->getOptionsModel())
        return;

    QMessageBox::warning(this, tr("Spend Zerocoin"),
                tr("Test for Spend. But better hodl !1!"),
                QMessageBox::Ok, QMessageBox::Ok);
            return;

        return;
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

void PrivacyDialog::setBalance(const CAmount& balance,         const CAmount& unconfirmedBalance, const CAmount& immatureBalance, 
                               const CAmount& zerocoinBalance, const CAmount& watchOnlyBalance,   const CAmount& watchUnconfBalance, 
                               const CAmount& watchImmatureBalance)
{
    currentBalance = balance;
    currentUnconfirmedBalance = unconfirmedBalance;
    currentImmatureBalance = immatureBalance;
    currentZerocoinBalance = zerocoinBalance;
    currentWatchOnlyBalance = watchOnlyBalance;
    currentWatchUnconfBalance = watchUnconfBalance;
    currentWatchImmatureBalance = watchImmatureBalance;

    CWalletDB walletdb(pwalletMain->strWalletFile);
    list<CZerocoinMint> listPubCoin = walletdb.ListMintedCoins(true);
 
    std::map<libzerocoin::CoinDenomination, CAmount> spread;
    for (const auto& denom : libzerocoin::zerocoinDenomList){
        spread.insert(std::pair<libzerocoin::CoinDenomination, CAmount>(denom, 0));
    }
    for (auto& mint : listPubCoin){
        spread.at(mint.GetDenomination())++;
    }
    
    int64_t nCoins = 0;
    for (const auto& m : libzerocoin::zerocoinDenomList) {
        nCoins = libzerocoin::ZerocoinDenominationToInt(m);
        switch (nCoins) {
            case libzerocoin::CoinDenomination::ZQ_ONE: 
                ui->labelzDenom1Amount->setText (QString::number(spread.at(m)));
                break;
            case libzerocoin::CoinDenomination::ZQ_FIVE:
                ui->labelzDenom2Amount->setText (QString::number(spread.at(m)));
                break;
            case libzerocoin::CoinDenomination::ZQ_TEN:
                ui->labelzDenom3Amount->setText (QString::number(spread.at(m)));
                break;
            case libzerocoin::CoinDenomination::ZQ_FIFTY:
                ui->labelzDenom4Amount->setText (QString::number(spread.at(m)));
                break;
            case libzerocoin::CoinDenomination::ZQ_ONE_HUNDRED:
                ui->labelzDenom5Amount->setText (QString::number(spread.at(m)));
                break;
            case libzerocoin::CoinDenomination::ZQ_FIVE_HUNDRED:
                ui->labelzDenom6Amount->setText (QString::number(spread.at(m)));
                break;
            case libzerocoin::CoinDenomination::ZQ_ONE_THOUSAND:
                ui->labelzDenom7Amount->setText (QString::number(spread.at(m)));
                break;
            case libzerocoin::CoinDenomination::ZQ_FIVE_THOUSAND:
                ui->labelzDenom8Amount->setText (QString::number(spread.at(m)));
                break;
            default:
                // Error Case: don't update display
                break;
        }
    }
    ui->labelzAvailableAmount->setText(QString::number(zerocoinBalance/COIN) + QString(" zPIV"));
    ui->labelzAvailableAmount_2->setText(QString::number(zerocoinBalance/COIN) + QString(" zPIV"));
    ui->labelzPIVAmountValue->setText(BitcoinUnits::floorHtmlWithUnit(nDisplayUnit, balance - immatureBalance, false, BitcoinUnits::separatorAlways));
}

void PrivacyDialog::updateDisplayUnit()
{
    if (walletModel && walletModel->getOptionsModel()) {
        nDisplayUnit = walletModel->getOptionsModel()->getDisplayUnit();
        if (currentBalance != -1)
            setBalance(currentBalance, currentUnconfirmedBalance, currentImmatureBalance, currentZerocoinBalance, 
                       currentWatchOnlyBalance, currentWatchUnconfBalance, currentWatchImmatureBalance);
    }
}

void PrivacyDialog::showOutOfSyncWarning(bool fShow)
{
    ui->labelzPIVSyncStatus->setVisible(fShow);
}
