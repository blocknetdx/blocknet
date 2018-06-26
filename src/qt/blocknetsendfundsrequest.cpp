// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetsendfundsrequest.h"

#include "optionsmodel.h"
#include "bitcoinunits.h"

#include <QMessageBox>

BlocknetSendFundsRequest::BlocknetSendFundsRequest(QWidget *widget, WalletModel *w, CCoinControl *coinControl,
                                                   QObject *parent) : QObject(parent), widget(widget), walletModel(w),
                                                                      coinControl(coinControl) { }

/**
 * @brief Sends the transaction.
 * @param walletTx Prepared tx
 * @param result
 * @return
 */
WalletModel::SendCoinsReturn BlocknetSendFundsRequest::send(QList<SendCoinsRecipient> &recipients, CAmount &txFees, CAmount &txAmount) {
    if (recipients.isEmpty())
        return WalletModel::InvalidAddress;

    // lambda wrapped in wallet unlock context
    auto sendInt = [=](CAmount &fees, CAmount &amount) -> WalletModel::SendCoinsReturn {
        int displayUnit = walletModel->getOptionsModel()->getDisplayUnit();
        WalletModelTransaction wtx(recipients);

        // Prepare tx
        CAmount payFee = 0;
        if (coinControl && coinControl->fOverrideFeeRate)
            payFee = coinControl->nMinimumTotalFee;
        auto result = walletModel->prepareTransaction(wtx, coinControl, payFee, true); // always sign tx for submit
        amount = wtx.getTotalTransactionAmount();
        fees = wtx.getTransactionFee();
        auto txFeeStr = BitcoinUnits::formatWithUnit(displayUnit, fees);

        // Check for failure
        QString msg = sendStatusMsg(result, txFeeStr, displayUnit);
        if (result.status != WalletModel::OK) {
            QMessageBox::warning(widget, tr("Issue"), msg);
            return result;
        }

        auto confmsg = tr("Are you sure you want to send?");
        confmsg.append("\n\n");
        confmsg.append(QString("%1: %2").arg(tr("Amount"), BitcoinUnits::formatWithUnit(displayUnit, wtx.getTotalTransactionAmount())));
        confmsg.append("\n");
        confmsg.append(QString("%1: %2").arg(tr("Fees"), txFeeStr));
        confmsg.append("\n");
        confmsg.append(QString("%1: %2 kB").arg(tr("Tx Size"), QString::number((double)wtx.getTransactionSize() / 1000)));
        confmsg.append("\n");
        if (recipients.count() > 1)
            confmsg.append(QString("%1: %2").arg(tr("Recipients"), QString::number(recipients.count())));
        else confmsg.append(QString("%1: %2").arg(tr("Recipient"), recipients[0].address));
        confmsg.append("\n\n");
        confmsg.append(QString("%1: %2").arg(tr("Total"), BitcoinUnits::formatWithUnit(displayUnit, amount + fees)));

        // Display confirm message box
        auto retval = static_cast<QMessageBox::StandardButton>(QMessageBox::question(widget, tr("Confirm send coins"),
                                                                                     confmsg, QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel));
        if (retval != QMessageBox::Yes) {
            return WalletModel::Cancel;
        }

        // Send the tx
        result = walletModel->sendCoins(wtx);
        QString statusMsg = sendStatusMsg(result, txFeeStr, displayUnit);
        if (result.status != WalletModel::OK) {
            QMessageBox::warning(widget, tr("Issue"), statusMsg);
            return result;
        }

        return result;
    };

    // Request to unlock the wallet
    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();
    if (encStatus == walletModel->Locked || encStatus == walletModel->UnlockedForAnonymizationOnly) {
        WalletModel::UnlockContext ctx(walletModel->requestUnlock(true));
        if (!ctx.isValid()) {
            // Unlock wallet was cancelled
            return WalletModel::Cancel;
        }
        return sendInt(txFees, txAmount);
    }

    return sendInt(txFees, txAmount);
}

QString BlocknetSendFundsRequest::sendStatusMsg(const WalletModel::SendCoinsReturn &scr, const QString &txFeeStr, const int displayUnit) {
    QString msg;
    switch (scr.status) {
        case WalletModel::InvalidAddress:
            msg = tr("The recipient address is not valid, please recheck.");
            break;
        case WalletModel::InvalidAmount:
            msg = tr("The amount to pay must be larger than 0.");
            break;
        case WalletModel::AmountExceedsBalance:
            msg = tr("The amount exceeds your balance.");
            break;
        case WalletModel::AmountWithFeeExceedsBalance:
            msg = tr("The total exceeds your balance when the %1 transaction fee is included.").arg(txFeeStr);
            break;
        case WalletModel::DuplicateAddress:
            msg = tr("Duplicate address found, can only send to each address once per send operation.");
            break;
        case WalletModel::TransactionCreationFailed:
            msg = tr("Transaction creation failed!");
            break;
        case WalletModel::TransactionCommitFailed:
            msg = tr("The transaction was rejected! This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.");
            break;
        case WalletModel::AnonymizeOnlyUnlocked:
            msg = tr("Error: The wallet was unlocked only to anonymize coins.");
            break;
        case WalletModel::InsaneFee:
            msg = tr("A fee %1 times higher than %2 per kB is considered an insanely high fee.").arg(10000)
                    .arg(BitcoinUnits::formatWithUnit(displayUnit, ::minRelayTxFee.GetFeePerK()));
            break;
        case WalletModel::OK:
        case WalletModel::Cancel:
        default:
            break;
    }
    return msg;
}
