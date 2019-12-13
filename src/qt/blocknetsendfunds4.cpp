// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetsendfunds4.h>

#include <qt/blocknetavatar.h>
#include <qt/blocknetcircle.h>
#include <qt/blocknetguiutil.h>
#include <qt/blocknethdiv.h>
#include <qt/blocknetsendfundsrequest.h>

#include <qt/optionsmodel.h>
#include <qt/sendcoinsdialog.h>

#include <util/system.h>
#include <validation.h>

#include <QKeyEvent>
#include <QMessageBox>

BlocknetSendFunds4::BlocknetSendFunds4(WalletModel *w, int id, QFrame *parent) : BlocknetSendFundsPage(w, id, parent),
                                                                                 layout(new QVBoxLayout) {
//    this->setStyleSheet("border: 1px solid red");
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(layout);
    layout->setContentsMargins(BGU::spi(15), BGU::spi(10), BGU::spi(35), BGU::spi(30));

    titleLbl = new QLabel(tr("Send Funds"));
    titleLbl->setObjectName("h4");

    auto *subtitleLbl = new QLabel(tr("Review Payment"));
    subtitleLbl->setObjectName("h2");

    auto *btnBox = new QFrame;
    auto *btnBoxLayout = new QHBoxLayout;
    btnBoxLayout->setContentsMargins(QMargins());
    btnBoxLayout->setSpacing(BGU::spi(15));
    btnBox->setLayout(btnBoxLayout);
    auto *backBtn = new BlocknetFormBtn;
    backBtn->setText(tr("Back"));
    continueBtn = new BlocknetFormBtn;
    continueBtn->setText(tr("Confirm Payment"));
    cancelBtn = new BlocknetFormBtn;
    cancelBtn->setObjectName("cancel");
    cancelBtn->setText(tr("Cancel"));
    btnBoxLayout->addWidget(backBtn, 0, Qt::AlignLeft | Qt::AlignBottom);
    btnBoxLayout->addWidget(continueBtn, 0, Qt::AlignLeft | Qt::AlignBottom);
    btnBoxLayout->addStretch(1);
    btnBoxLayout->addWidget(cancelBtn, 0, Qt::AlignRight | Qt::AlignBottom);

    content = new QFrame;
    content->setObjectName("review");
    content->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
    contentLayout = new QVBoxLayout;
    contentLayout->setContentsMargins(QMargins());
    content->setLayout(contentLayout);

    // Totals box
    auto *totals = new QFrame;
    totals->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    auto *totalsLayout = new QGridLayout;
    totalsLayout->setContentsMargins(0, 0, BGU::spi(35), 0);
    totalsLayout->setSpacing(BGU::spi(50));
    totalsLayout->setVerticalSpacing(0);
    totals->setLayout(totalsLayout);

    auto *div1 = new BlocknetHDiv;
    auto *feeLbl = new QLabel(tr("Transaction Fee")); feeLbl->setObjectName("header");
    feeValueLbl = new QLabel; feeValueLbl->setObjectName("standard");

    auto *div2 = new BlocknetHDiv;
    auto *totalLbl = new QLabel(tr("Total")); totalLbl->setObjectName("header");
    totalValueLbl = new QLabel; totalValueLbl->setObjectName("header");

    auto *pl1 = new QLabel;
    auto *pl2 = new QLabel;

    totalsLayout->addWidget(div1, 0, 0, 1, 3); totalsLayout->setRowMinimumHeight(0, BGU::spi(15));
    totalsLayout->addWidget(feeLbl, 1, 0, Qt::AlignLeft | Qt::AlignVCenter);
    totalsLayout->addWidget(feeValueLbl, 1, 1, Qt::AlignLeft | Qt::AlignVCenter);
    totalsLayout->addWidget(pl1, 1, 2, Qt::AlignRight);
    totalsLayout->addWidget(div2, 2, 0, 1, 3); totalsLayout->setRowMinimumHeight(2, BGU::spi(15));
    totalsLayout->addWidget(totalLbl, 3, 0, Qt::AlignLeft | Qt::AlignVCenter);
    totalsLayout->addWidget(totalValueLbl, 3, 1, Qt::AlignLeft | Qt::AlignVCenter);
    totalsLayout->addWidget(pl2, 3, 2, Qt::AlignRight);
    totalsLayout->setColumnStretch(1, 1);
    totalsLayout->setColumnStretch(2, 1);
    totalsLayout->setRowMinimumHeight(1, BGU::spi(45));
    totalsLayout->setRowMinimumHeight(3, BGU::spi(45));
    // End Totals box

    warningLbl = new QLabel;
    warningLbl->setObjectName("warning");

    layout->addWidget(titleLbl, 0, Qt::AlignTop | Qt::AlignLeft);
    layout->addSpacing(BGU::spi(30));
    layout->addWidget(subtitleLbl, 0, Qt::AlignTop);
    layout->addSpacing(BGU::spi(30));
    layout->addWidget(content, 10);
    layout->addSpacing(BGU::spi(10));
    layout->addWidget(totals);
    layout->addSpacing(BGU::spi(10));
    layout->addWidget(warningLbl);
    layout->addSpacing(BGU::spi(10));
    layout->addWidget(btnBox);

    connect(continueBtn, &BlocknetFormBtn::clicked, this, &BlocknetSendFunds4::onSubmit);
    connect(cancelBtn, &BlocknetFormBtn::clicked, this, &BlocknetSendFunds4::onCancel);
    connect(backBtn, &BlocknetFormBtn::clicked, this, &BlocknetSendFunds4::onBack);
}

/**
 * @brief Builds the summary list.
 * @param data
 */
void BlocknetSendFunds4::setData(BlocknetSendFundsModel *model) {
    BlocknetSendFundsPage::setData(model);
    displayUnit = walletModel->getOptionsModel()->getDisplayUnit();
    displayMultiple();
    if (!walletModel->wallet().isLocked()) {
        CCoinControl cc; model->prepareFunds(walletModel, cc);
    }
    fillWalletData();
}

void BlocknetSendFunds4::onEncryptionStatus() {
    if (!this->isHidden() && walletModel && !walletModel->wallet().isLocked()) {
        CCoinControl cc; model->prepareFunds(walletModel, cc);
        fillWalletData();
    }
}

/**
 * Update labels with latest state.
 */
void BlocknetSendFunds4::fillWalletData() {
    // Disable the confirm button if there are any problems
    continueBtn->setDisabled(model->txStatus().status != WalletModel::OK);

    if (model->txStatus().status != WalletModel::OK) {
        // Handle errors
        feeValueLbl->setText(tr("n/a"));
        totalValueLbl->setText(tr("n/a"));
        auto status = model->txStatus();
        warningLbl->setText(BlocknetSendFundsRequest::sendStatusMsg(status, "", displayUnit));
        return;
    }

    warningLbl->clear();

    CAmount fees = walletModel->wallet().isLocked() ? model->estimatedFees() : model->txFees();
    CAmount total = model->txTotalAmount();
    feeValueLbl->setText(feeText(BitcoinUnits::formatWithUnit(displayUnit, fees)));
    if (model->subtractFee())
        total -= fees;
    totalValueLbl->setText(totalText(BitcoinUnits::formatWithUnit(displayUnit, fees + total)));
}

/**
 * @brief Clears the summary list.
 */
void BlocknetSendFunds4::clear() {
    clearRecipients();
}

/**
 * @brief  Validates the transaction details. Fails if there are no transactions, or invalid addresses and amounts.
 * @return
 */
bool BlocknetSendFunds4::validated() {
    if (model->txRecipients().isEmpty()) {
        auto msg = tr("Please add recipients.");
        QMessageBox::warning(this->parentWidget(), tr("Issue"), msg);
        return false;
    }

    bool allValid = [](const QList<SendCoinsRecipient> & recs, WalletModel *w) -> bool {
        for (auto & r : recs) {
            if (!w->validateAddress(r.address) || r.amount <= 0)
                return false;
        }
        return true;
    }(model->txRecipients(), walletModel);

    if (model->txRecipients().isEmpty() || !allValid) {
        auto msg = tr("Please specify an amount larger than %1 for each address.")
                           .arg(BitcoinUnits::formatWithUnit(displayUnit, ::minRelayTxFee.GetFeePerK()));
        QMessageBox::warning(this->parentWidget(), tr("Issue"), msg);
        return false;
    }

    return true;
}

void BlocknetSendFunds4::keyPressEvent(QKeyEvent *event) {
    QWidget::keyPressEvent(event);
    if (this->isHidden())
        return;
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
        onSubmit();
    else if (event->key() == Qt::Key_Escape)
        onBack();
}

void BlocknetSendFunds4::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    connect(walletModel, &WalletModel::encryptionStatusChanged, this, &BlocknetSendFunds4::onEncryptionStatus);
    connect(walletModel->getOptionsModel(), &OptionsModel::displayUnitChanged, this, &BlocknetSendFunds4::onDisplayUnit);
}

void BlocknetSendFunds4::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    disconnect(walletModel, &WalletModel::encryptionStatusChanged, this, &BlocknetSendFunds4::onEncryptionStatus);
    disconnect(walletModel->getOptionsModel(), &OptionsModel::displayUnitChanged, this, &BlocknetSendFunds4::onDisplayUnit);
}

/**
 * @brief Draws the summary list. The amounts are formatted with the BitcoinUnits utilities.
 */
void BlocknetSendFunds4::displayMultiple() {
    clearRecipients();

    recipients = new QFrame;
    recipients->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *recipientsLayout = new QVBoxLayout;
    recipientsLayout->setContentsMargins(QMargins());
    recipients->setLayout(recipientsLayout);

    auto *scrollc = new QFrame;
    scrollc->setObjectName("content");
    scrollc->setContentsMargins(0, 0, BGU::spi(20), 0);
    scrollc->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum);
    auto *scrollcLayout = new QGridLayout;
    scrollcLayout->setContentsMargins(QMargins());
    scrollcLayout->setSpacing(BGU::spi(50));
    scrollcLayout->setVerticalSpacing(0);
    scrollc->setLayout(scrollcLayout);

    // Support scrollable content
    scrollArea = new QScrollArea;
    scrollArea->setObjectName("contentScrollArea");
    scrollArea->setContentsMargins(QMargins());
    scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(scrollc);

    // Add headers
    QVector<QString> headers = { tr("Recipients"), tr("Amount") };
    for (int i = 0; i < headers.count(); ++i) {
        auto *headerLbl = new QLabel(headers[i]);
        headerLbl->setObjectName("header");
        scrollcLayout->addWidget(headerLbl, 0, i, Qt::AlignLeft);
        scrollcLayout->setRowMinimumHeight(0, BGU::spi(40));
    }

    // Add empty transaction struct to simulate recipients dividers
    const auto drawRecipients = model->txRecipients();

    // Display summaries of all addresses (in recipients)
    int i;
    for (i = 0; i < drawRecipients.count(); ++i) {
        int row = i + 1; // account for header row

        auto recipient = drawRecipients[i];

        if (recipient.address.isEmpty()) {
            auto *div = new BlocknetHDiv;
            scrollcLayout->addWidget(div, row, 0, 1, 3, Qt::AlignVCenter);
            scrollcLayout->setRowMinimumHeight(row, BGU::spi(15));
            continue;
        }

        auto *box = new QFrame;
        box->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
        auto *boxLayout = new QHBoxLayout;
        boxLayout->setContentsMargins(QMargins());
        box->setLayout(boxLayout);
        auto *addrLbl = new QLabel(recipient.label.isEmpty() ? recipient.address : recipient.label);
        addrLbl->setObjectName("standard");
        auto *avatar = new BlocknetAvatar(recipient.address, BGU::spi(34), BGU::spi(34));
        boxLayout->addWidget(avatar, 0, Qt::AlignVCenter);
        boxLayout->addWidget(addrLbl, 1);
        scrollcLayout->addWidget(box, row, 0, Qt::AlignLeft | Qt::AlignVCenter); scrollcLayout->setColumnStretch(0, 0);

        auto *amountLbl = new QLabel;
        amountLbl->setObjectName("standard");
        amountLbl->setText(BlocknetSendFundsModel::intToString(model->txAmountMinusFee(recipient.amount), displayUnit));
        scrollcLayout->addWidget(amountLbl, row, 1, Qt::AlignLeft | Qt::AlignVCenter); scrollcLayout->setColumnStretch(0, 1);

        auto *edit = new QPushButton(tr("Edit"));
        edit->setObjectName("linkBtn");
        edit->setCursor(Qt::PointingHandCursor);
        connect(edit, &QPushButton::clicked, this, &BlocknetSendFunds4::onEdit);
        scrollcLayout->addWidget(edit, row, 10, Qt::AlignRight | Qt::AlignVCenter);  scrollcLayout->setColumnStretch(0, 2);

        scrollcLayout->setRowMinimumHeight(row, BGU::spi(40));
    }
    auto *spacer = new QFrame;
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    scrollcLayout->addWidget(spacer, i + 1, 0, 1, 3);

    recipientsLayout->addWidget(scrollArea, 10);
    contentLayout->addWidget(recipients, 1);

    CAmount fees = walletModel->wallet().isLocked() ? model->estimatedFees() : model->txFees();
    CAmount total = model->txTotalAmount();
    feeValueLbl->setText(feeText(BitcoinUnits::formatWithUnit(displayUnit, fees)));
    if (model->subtractFee())
        total -= fees;
    totalValueLbl->setText(totalText(BitcoinUnits::formatWithUnit(displayUnit, (walletModel->wallet().isLocked() ? fees : 0) + total)));
}

/**
 * @brief Clears the summary data, including removing the recipients list.
 */
void BlocknetSendFunds4::clearRecipients() {
    if (recipients == nullptr)
        return;
    totalValueLbl->clear();
    feeValueLbl->clear();
    warningLbl->clear();
    contentLayout->removeWidget(recipients);
    recipients->deleteLater();
    scrollArea->deleteLater();
    recipients = nullptr;
    scrollArea = nullptr;
}

void BlocknetSendFunds4::onEdit() {
    Q_EMIT edit();
}

void BlocknetSendFunds4::onSubmit() {
    if (!this->validated())
        return;

    // Use lambda to encapsulate the send request in a wallet unlock context
    auto send = [this]() {
        if (warningLbl) warningLbl->clear();
        bool canceledSend{false};
        WalletModel::SendCoinsReturn sendStatus = model->txStatus();
        const auto feeMsg = BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(),
                                                         model->txFees());
        if (!model->hasWalletTx()) { // if we haven't prepared the tx yet, do it now
            CCoinControl cc = model->getCoinControl(walletModel);
            sendStatus = model->prepareFunds(walletModel, cc);
        }

        // Check for bad status
        if (sendStatus.status != WalletModel::OK) {
            // process prepareStatus and on error generate message shown to user
            auto res = BlocknetSendFundsPage::processSendCoinsReturn(walletModel, sendStatus, feeMsg);
            if (res.second)
                QMessageBox::critical(this->parentWidget(), tr("Issue"), res.first);
            else
                QMessageBox::warning(this->parentWidget(), tr("Issue"), res.first);
        } else {
            CAmount txFee = model->walletTx()->getTransactionFee();
            // Format confirmation message
            QStringList formatted;
            QString questionString = tr("Are you sure you want to send?");
            questionString.append("<br /><span style='font-size:10pt;'>");
            questionString.append(tr("Please, review your transaction."));
            questionString.append("</span><br />%1");
            if (txFee > 0) {
                // append fee string if a fee is required
                questionString.append("<hr /><b>");
                questionString.append(tr("Transaction fee"));
                questionString.append("</b>");
                // append transaction size
                questionString.append(" (" + QString::number((double)model->walletTx()->getTransactionSize() / 1000) + " kB): ");
                // append transaction fee value
                questionString.append("<span style='color:#aa0000; font-weight:bold;'>");
                questionString.append(BitcoinUnits::formatHtmlWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), txFee));
                questionString.append("</span><br />");
            }
            // add total amount in all subdivision units
            questionString.append("<hr />");
            CAmount totalAmount = model->walletTx()->getTotalTransactionAmount() + txFee;
            questionString.append(QString("<b>%1</b>: <b>%2</b>").arg(tr("Total Amount"))
                    .arg(BitcoinUnits::formatHtmlWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), totalAmount)));
            SendConfirmationDialog confirmationDialog(tr("Confirm send coins"),
                    questionString.arg(formatted.join("<br />")), SEND_CONFIRM_DELAY, this);
            confirmationDialog.exec();
            auto retval = static_cast<QMessageBox::StandardButton>(confirmationDialog.result());
            if (retval == QMessageBox::Yes) {
                // now send the prepared transaction
                sendStatus = walletModel->sendCoins(*model->walletTx());
                // process sendStatus and on error generate message shown to user
                auto res = BlocknetSendFundsPage::processSendCoinsReturn(walletModel, sendStatus, feeMsg);
                if (sendStatus.status != WalletModel::OK) {
                    if (res.second)
                        QMessageBox::critical(this->parentWidget(), tr("Issue"), res.first);
                    else
                        QMessageBox::warning(this->parentWidget(), tr("Issue"), res.first);
                }
            } else
                canceledSend = true;
        }

        if (!canceledSend && sendStatus.status != WalletModel::OK) {
            if (feeValueLbl) feeValueLbl->setText(tr("n/a"));
            if (totalValueLbl) totalValueLbl->setText(tr("n/a"));
            if (warningLbl) warningLbl->setText(BlocknetSendFundsRequest::sendStatusMsg(sendStatus, "", displayUnit));
            return;
        }

        displayMultiple();
        fillWalletData();

        if (!canceledSend && sendStatus.status == WalletModel::OK)
            Q_EMIT submit();
    };

    // Unlock wallet context (for relock)
    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();
    if (encStatus == WalletModel::EncryptionStatus::Locked || util::unlockedForStakingOnly) {
        const bool stateUnlockForStaking = util::unlockedForStakingOnly;
        WalletModel::UnlockContext ctx(walletModel->requestUnlock());
        if (!ctx.isValid() || util::unlockedForStakingOnly) { // Unlock wallet was cancelled
            if (warningLbl) warningLbl->setText(tr("Wallet unlock failed, please try again"));
        } else {
            send();
            util::unlockedForStakingOnly = stateUnlockForStaking; // restore state after send
        }
        return;
    }

    // wallet is already unlocked
    send();
}

void BlocknetSendFunds4::onDisplayUnit(int unit) {
    displayUnit = unit;
    if (walletModel && !walletModel->wallet().isLocked()) {
        CCoinControl cc; model->prepareFunds(walletModel, cc);
    }
    displayMultiple();
    fillWalletData();
}

QString BlocknetSendFunds4::feeText(QString fee) {
    if (!model->customFee()) {
        // The fee is an estimate if the wallet is locked
        bool estimatingFee = walletModel->wallet().isLocked();
        QString f = (estimatingFee ? tr("(estimated since wallet is locked)") : "");
        if (estimatingFee)
            return QString("%1 %2").arg(fee, f);
    }
    return fee;
}

QString BlocknetSendFunds4::totalText(QString total) {
    if (model->subtractFee())
        return QString("%1 %2").arg(total, tr("(including fee)"));
    return total;
}
