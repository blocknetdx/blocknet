// Copyright (c) 2011-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "privacydialog.h"
#include "ui_privacydialog.h"

#include "addressbookpage.h"
#include "addresstablemodel.h"
#include "bitcoinunits.h"
#include "coincontroldialog.h"
#include "libzerocoin/Denominations.h"
#include "optionsmodel.h"
#include "sendcoinsentry.h"
#include "walletmodel.h"
#include "coincontrol.h"

#include <QClipboard>
#include <QSettings>

PrivacyDialog::PrivacyDialog(QWidget* parent) : QDialog(parent),
                                                          ui(new Ui::PrivacyDialog),
                                                          walletModel(0),
                                                          currentBalance(-1)
{
    nDisplayUnit = 0; // just make sure it's not unitialized
    ui->setupUi(this);

    // "Spending 999999 zPIV ought to be enough for anybody." - Bill Gates, 2017
    ui->zPIVpayAmount->setValidator( new QIntValidator(0, 999999, this) );
    ui->labelMintAmountValue->setValidator( new QIntValidator(0, 999999, this) );

    // Default texts for (mini-) coincontrol
    ui->labelCoinControlQuantity->setText (tr("Coins automatically selected"));
    ui->labelCoinControlAmount->setText (tr("Coins automatically selected"));
    ui->labelzPIVSyncStatus->setText("(" + tr("out of sync") + ")");

    // Sunken frame for minting messages
    ui->TEMintStatus->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    ui->TEMintStatus->setLineWidth (2);
    ui->TEMintStatus->setMidLineWidth (2);
    ui->TEMintStatus->setPlainText(tr("Mint Status: Okay"));

    // Coin Control signals
    connect(ui->pushButtonCoinControl, SIGNAL(clicked()), this, SLOT(coinControlButtonClicked()));

    // Coin Control: clipboard actions
    QAction* clipboardQuantityAction = new QAction(tr("Copy quantity"), this);
    QAction* clipboardAmountAction = new QAction(tr("Copy amount"), this);
    connect(clipboardQuantityAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardQuantity()));
    connect(clipboardAmountAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardAmount()));
    ui->labelCoinControlQuantity->addAction(clipboardQuantityAction);
    ui->labelCoinControlAmount->addAction(clipboardAmountAction);
    
    // PIVX settings
    QSettings settings;
    if (!settings.contains("nSecurityLevel")){
        nSecurityLevel = 42;
        settings.setValue("nSecurityLevel", nSecurityLevel);
    }
    else{
        nSecurityLevel = settings.value("nSecurityLevel").toInt();
    }

    // Start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);

    // Hide those placeholder elements needed for CoinControl interaction
    ui->WarningLabel->hide();    // Explanatory text visible in QT-Creator
    ui->dummyHideWidget->hide(); // Dummy widget with elements to hide

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
        ui->securityLevel->setValue(nSecurityLevel);
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
    ui->TEMintStatus->setPlainText(tr("Mint Status: Okay"));
    
    // Wallet must be unlocked for minting
    if (pwalletMain->IsLocked()){
        ui->TEMintStatus->setPlainText(tr("Error: your wallet is locked. Please enter the wallet passphrase first."));
        return;
    }

    QString sAmount = ui->labelMintAmountValue->text();
    CAmount nAmount = sAmount.toInt() * COIN;

    // Minting amount must be > 0
    if(nAmount <= 0){
        ui->TEMintStatus->setPlainText(tr("Message: Enter an amount > 0."));
        return;
    }

    ui->TEMintStatus->setPlainText(tr("Minting ") + ui->labelMintAmountValue->text() + " zPIV...");
    ui->TEMintStatus->repaint ();
    
    int64_t nTime = GetTimeMillis();
    
    CWalletTx wtx;
    vector<CZerocoinMint> vMints;
    string strError = pwalletMain->MintZerocoin(nAmount, wtx, vMints, CoinControlDialog::coinControl);
    
    // Return if something went wrong during minting
    if (strError != ""){
        ui->TEMintStatus->setPlainText(QString::fromStdString(strError));
        return;
    }

    int64_t nDuration = GetTimeMillis() - nTime;
    
    // Minting successfully finished. Show some stats for entertainment.
    QString strStatsHeader = tr("Successfully minted ") + ui->labelMintAmountValue->text() + tr(" zPIV in ") + 
                             QString::number(nDuration) + tr(" ms. Used denominations:\n");
    
    // Clear amount to avoid double spending when accidentally clicking twice
    ui->labelMintAmountValue->setText ("0");
            
    QString strStats = "";
    ui->TEMintStatus->setPlainText(strStatsHeader);

    for (CZerocoinMint mint : vMints) {
        boost::this_thread::sleep(boost::posix_time::milliseconds(100));
        strStats = strStats + QString::number(mint.GetDenomination()) + " ";
        ui->TEMintStatus->setPlainText(strStatsHeader + strStats);
        ui->TEMintStatus->repaint ();
        
    }

    // Available balance isn't always updated, so force it.
    setBalance(walletModel->getBalance(),         walletModel->getUnconfirmedBalance(), walletModel->getImmatureBalance(), 
               walletModel->getZerocoinBalance(), walletModel->getWatchBalance(),       walletModel->getWatchUnconfirmedBalance(), 
               walletModel->getWatchImmatureBalance());
    coinControlUpdateLabels();

    return;
}

void PrivacyDialog::on_pushButtonMintReset_clicked()
{
    if (!walletModel || !walletModel->getOptionsModel())
        return;

    ui->TEMintStatus->setPlainText(tr("Starting ResetMintZerocoin: rescanning complete blockchain, this will need several minutes depending on your hardware. \nPlease be patient..."));
    ui->TEMintStatus->repaint ();
    int64_t nTime = GetTimeMillis();
    string strResetMintResult = pwalletMain->ResetMintZerocoin();
    int64_t nDuration = GetTimeMillis() - nTime;
    ui->TEMintStatus->setPlainText(QString::fromStdString(strResetMintResult) + tr("Duration: ") + QString::number(nDuration) + tr(" ms.\n"));
    ui->TEMintStatus->repaint ();

    return;
}

void PrivacyDialog::on_pushButtonSpentReset_clicked()
{
    if (!walletModel || !walletModel->getOptionsModel())
        return;

    ui->TEMintStatus->setPlainText(tr("Starting ResetSpentZerocoin: "));
    ui->TEMintStatus->repaint ();
    int64_t nTime = GetTimeMillis();
    string strResetSpentResult = pwalletMain->ResetSpentZerocoin();
    int64_t nDuration = GetTimeMillis() - nTime;
    ui->TEMintStatus->setPlainText(QString::fromStdString(strResetSpentResult) + tr("Duration: ") + QString::number(nDuration) + tr(" ms.\n"));
    ui->TEMintStatus->repaint ();

    return;
}

void PrivacyDialog::on_pushButtonSpendzPIV_clicked()
{
    QSettings settings;

    if (!walletModel || !walletModel->getOptionsModel() || !pwalletMain)
        return;

    // Request unlock if wallet was locked or unlocked for mixing:
    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();
    if (encStatus == walletModel->Locked || encStatus == walletModel->UnlockedForAnonymizationOnly) {
        WalletModel::UnlockContext ctx(walletModel->requestUnlock(true));
        if (!ctx.isValid()) {
            // Unlock wallet was cancelled
            return;
        }
    }

    // Handle 'Pay To' address options
    CBitcoinAddress address(ui->payTo->text().toStdString());
    if(ui->payTo->text().isEmpty()){
        QMessageBox::information(this, tr("Spend Zerocoin"), tr("No 'Pay To' address provided, creating local payment"), QMessageBox::Ok, QMessageBox::Ok);
    }
    else{
        if (!address.IsValid()) {
            QMessageBox::warning(this, tr("Spend Zerocoin"), tr("Invalid Pivx Address"), QMessageBox::Ok, QMessageBox::Ok);
            return;
        }
    }

    //grab as a double, increase by COIN and cutoff any remainder by assigning it as int64_t/CAmount
    double dAmount = ui->zPIVpayAmount->text().toDouble();
    CAmount nAmount = dAmount * COIN;
    if (!MoneyRange(nAmount) || nAmount < 1) {
        QMessageBox::warning(this, tr("Spend Zerocoin"), tr("Invalid Send Amount"), QMessageBox::Ok, QMessageBox::Ok);
        return;
    }

    // Persist Security Level for next start
    nSecurityLevel = ui->securityLevel->value();
    settings.setValue("nSecurityLevel", nSecurityLevel);

    // Convert change to zPIV
    bool fMintChange = ui->checkBoxMintChange->isChecked();

    // Spend confirmation message box

    // Add address info if available
    QString strAddressLabel = "";
    if(!ui->payTo->text().isEmpty() && !ui->addAsLabel->text().isEmpty()){
        strAddressLabel = " (" + ui->addAsLabel->text() + ") ";        
    }

    CAmount nDisplayAmount = nAmount / COIN;
    QString strQuestionString = tr("Are you sure you want to send?<br /><br />");
    QString strAmount = "<b>" + QString::number(nDisplayAmount, 10) + " zPIV</b>";
    QString strAddress = tr(" to address ") + QString::fromStdString(address.ToString()) + strAddressLabel + " <br />";

    if(ui->payTo->text().isEmpty()){
        // No address provided => send to local address
        strAddress = tr(" to a newly generated (unused and therefor anonymous) local address <br />");
    }

    QString strSecurityLevel = tr("with Security Level ") + ui->securityLevel->text() + " ?";

    strQuestionString = strQuestionString + strAmount + strAddress + strSecurityLevel;

    // Display message box
    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm send coins"),
        strQuestionString,
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if (retval != QMessageBox::Yes) {
        // Sending canceled
        return;
    }

    // attempt to spend the zPiv
    CWalletTx wtxNew;
    vector<CZerocoinMint> vMintsSelected;
    vector<CZerocoinSpend> vSpends;
    
    int64_t nTime = GetTimeMillis();
    ui->TEMintStatus->setPlainText(tr("Spending Zerocoin.\nComputationally expensive, might need several minutes depending on the selected Security Level and your hardware. \nPlease be patient..."));
    ui->TEMintStatus->repaint();
    int nStatus;
    string strError = pwalletMain->SpendZerocoin(nAmount, nSecurityLevel, wtxNew, vSpends, vMintsSelected, fMintChange, nStatus, &address);

    if (strError != "") {
        QMessageBox::warning(this, tr("Spend Zerocoin"), tr(strError.c_str()), QMessageBox::Ok, QMessageBox::Ok);
        ui->TEMintStatus->setPlainText(tr("Spend Zerocoin Failed!"));
        ui->TEMintStatus->repaint();
        return;
    }

    QString strStats = "";
    CAmount nValueIn = 0;
    int nCount = 0;
    for (CZerocoinSpend spend : vSpends) {
        strStats += tr("zPiv Spend #: ") + QString::number(nCount) + ", ";
        strStats += tr("denomination: ") + QString::number(spend.GetDenomination()) + ", ";
        strStats += tr("serial: ") + spend.GetSerial().ToString().c_str() + "\n";
        nValueIn += libzerocoin::ZerocoinDenominationToAmount(spend.GetDenomination());
    }

    CAmount nValueOut = 0;
    for (const CTxOut& txout: wtxNew.vout) {
        strStats += tr("value out: ") + QString::number(txout.nValue/COIN) + " Piv, ";
        nValueOut += txout.nValue;

        strStats += tr("address: ");
        CTxDestination dest;
        if(txout.scriptPubKey.IsZerocoinMint())
            strStats += tr("zPiv Mint");
        else if(ExtractDestination(txout.scriptPubKey, dest))
            strStats += tr(CBitcoinAddress(dest).ToString().c_str());
        strStats += "\n";
    }
    int64_t nDuration = GetTimeMillis() - nTime;
    strStats += tr("Duration: ") + QString::number(nDuration) + tr(" ms.\n");

    QString strReturn;
    strReturn += tr("txid: ") + wtxNew.GetHash().ToString().c_str() + "\n";
    strReturn += tr("fee: ") + QString::number((nValueIn-nValueOut)/COIN) + "\n";
    strReturn += strStats;

    // Clear amount to avoid double spending when accidentally clicking twice
    ui->zPIVpayAmount->setText ("0");

    ui->TEMintStatus->setPlainText(strReturn);
    ui->TEMintStatus->repaint();
}

void PrivacyDialog::on_payTo_textChanged(const QString& address)
{
    updateLabel(address);
}

// Coin Control: copy label "Quantity" to clipboard
void PrivacyDialog::coinControlClipboardQuantity()
{
    GUIUtil::setClipboard(ui->labelCoinControlQuantity->text());
}

// Coin Control: copy label "Amount" to clipboard
void PrivacyDialog::coinControlClipboardAmount()
{
    GUIUtil::setClipboard(ui->labelCoinControlAmount->text().left(ui->labelCoinControlAmount->text().indexOf(" ")));
}

// Coin Control: button inputs -> show actual coin control dialog
void PrivacyDialog::coinControlButtonClicked()
{
    CoinControlDialog dlg;
    dlg.setModel(walletModel);
    dlg.exec();
    coinControlUpdateLabels();
}

// Coin Control: update labels
void PrivacyDialog::coinControlUpdateLabels()
{
    if (!walletModel || !walletModel->getOptionsModel() || !walletModel->getOptionsModel()->getCoinControlFeatures())
        return;

     // set pay amounts
    CoinControlDialog::payAmounts.clear();

    if (CoinControlDialog::coinControl->HasSelected()) {
        // Actual coin control calculation
        CoinControlDialog::updateLabels(walletModel, this);
    } else {
        ui->labelCoinControlQuantity->setText (tr("Coins automatically selected"));
        ui->labelCoinControlAmount->setText (tr("Coins automatically selected"));
    }
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
                ui->labelzDenom1Amount->setText (QString::number(spread.at(m)) + QString(" x "));
                break;
            case libzerocoin::CoinDenomination::ZQ_FIVE:
                ui->labelzDenom2Amount->setText (QString::number(spread.at(m)) + QString(" x "));
                break;
            case libzerocoin::CoinDenomination::ZQ_TEN:
                ui->labelzDenom3Amount->setText (QString::number(spread.at(m)) + QString(" x "));
                break;
            case libzerocoin::CoinDenomination::ZQ_FIFTY:
                ui->labelzDenom4Amount->setText (QString::number(spread.at(m)) + QString(" x "));
                break;
            case libzerocoin::CoinDenomination::ZQ_ONE_HUNDRED:
                ui->labelzDenom5Amount->setText (QString::number(spread.at(m)) + QString(" x "));
                break;
            case libzerocoin::CoinDenomination::ZQ_FIVE_HUNDRED:
                ui->labelzDenom6Amount->setText (QString::number(spread.at(m)) + QString(" x "));
                break;
            case libzerocoin::CoinDenomination::ZQ_ONE_THOUSAND:
                ui->labelzDenom7Amount->setText (QString::number(spread.at(m)) + QString(" x "));
                break;
            case libzerocoin::CoinDenomination::ZQ_FIVE_THOUSAND:
                ui->labelzDenom8Amount->setText (QString::number(spread.at(m)) + QString(" x "));
                break;
            default:
                // Error Case: don't update display
                break;
        }
    }
    ui->labelzAvailableAmount->setText(QString::number(zerocoinBalance/COIN) + QString(" zPIV "));
    ui->labelzAvailableAmount_2->setText(QString::number(zerocoinBalance/COIN) + QString(" zPIV "));
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
