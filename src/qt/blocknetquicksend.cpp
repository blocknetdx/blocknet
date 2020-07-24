// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetquicksend.h>

#include <qt/blocknetaddressbook.h>
#include <qt/blocknethdiv.h>
#include <qt/blockneticonbtn.h>
#include <qt/blocknetsendfundsrequest.h>
#include <qt/blocknetsendfundsutil.h>

#include <qt/addresstablemodel.h>
#include <qt/optionsmodel.h>
#include <qt/sendcoinsdialog.h>

#include <amount.h>
#include <base58.h>
#include <wallet/coincontrol.h>
#include <validation.h>

#include <QMessageBox>
#include <QKeyEvent>

BlocknetQuickSend::BlocknetQuickSend(WalletModel *w, QWidget *parent) : QFrame(parent), walletModel(w),
                                                                        layout(new QVBoxLayout) {
//    this->setStyleSheet("border: 1px solid red");
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(layout);
    layout->setContentsMargins(BGU::spi(45), BGU::spi(10), BGU::spi(45), BGU::spi(30));

    displayUnit = walletModel->getOptionsModel()->getDisplayUnit();
    auto displayUnitName = BitcoinUnits::longName(displayUnit);

    titleLbl = new QLabel(tr("Quick Send"));
    titleLbl->setObjectName("h4");

    auto *subtitleLbl = new QLabel(tr("Who would you like to send funds to?"));
    subtitleLbl->setObjectName("h2");

    auto *addrBox = new QFrame;
    addrBox->setContentsMargins(QMargins());
    auto *addrBoxLayout = new QHBoxLayout;
    addrBoxLayout->setContentsMargins(QMargins());
    addrBox->setLayout(addrBoxLayout);
    addressTi = new BlocknetLineEdit(BGU::spi(400));
    addressTi->setObjectName("address");
    addressTi->setPlaceholderText(tr("Enter Blocknet Address..."));
    addressTi->setFocusPolicy(Qt::FocusPolicy::StrongFocus);
    addressTi->setValidator(new QRegExpValidator(QRegExp("[a-zA-Z0-9]{33,35}"), this));
    auto *addAddressBtn = new BlocknetIconBtn(QString(), ":/redesign/QuickActions/AddressBookIcon.png");
    addrBoxLayout->addWidget(addressTi, 0, Qt::AlignTop);
    addrBoxLayout->addSpacing(BGU::spi(20));
    addrBoxLayout->addWidget(addAddressBtn, 0, Qt::AlignTop);

    auto *amountLbl = new QLabel(tr("How much would you like to send?"));
    amountLbl->setObjectName("h2");
    auto *amountBox = new QFrame;
    auto *amountBoxLayout = new QHBoxLayout;
    amountBoxLayout->setContentsMargins(QMargins());
    amountBox->setLayout(amountBoxLayout);
    amountTi = new BlocknetLineEdit;
    amountTi->setPlaceholderText(tr("Enter Amount..."));
    amountTi->setValidator(new BlocknetNumberValidator(0, BLOCKNETGUI_FUNDS_MAX, BitcoinUnits::decimals(displayUnit)));
    amountTi->setMaxLength(BLOCKNETGUI_MAXCHARS);
    auto *coinLbl = new QLabel(displayUnitName);
    coinLbl->setObjectName("coin");
    coinLbl->setFixedHeight(amountTi->minimumHeight());
    amountBoxLayout->addWidget(amountTi, 0, Qt::AlignLeft);
    amountBoxLayout->addWidget(coinLbl, 0, Qt::AlignLeft);
    amountBoxLayout->addStretch(1);

    // address div
    auto *div1 = new BlocknetHDiv;
    // grid divs
    auto *gdiv1 = new BlocknetHDiv;
    auto *gdiv2 = new BlocknetHDiv;
    auto *gdiv3 = new BlocknetHDiv;

    // Display total
    auto *totalGrid = new QFrame;
//    totalGrid->setStyleSheet("border: 1px solid red");
    totalGrid->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *totalsLayout = new QGridLayout;
    totalsLayout->setContentsMargins(QMargins());
    totalsLayout->setSpacing(BGU::spi(50));
    totalsLayout->setVerticalSpacing(0);
    totalGrid->setLayout(totalsLayout);

    auto *feeLbl = new QLabel(tr("Transaction Fee"));
    feeLbl->setObjectName("header");
    feeValueLbl = new QLabel;
    feeValueLbl->setObjectName("standard");
    auto *feeCol3 = new QLabel;

    auto *totalLbl = new QLabel(tr("Total"));
    totalLbl->setObjectName("header");
    totalValueLbl = new QLabel(BitcoinUnits::formatWithUnit(displayUnit, 0));
    totalValueLbl->setObjectName("header");
    auto *totalCol3 = new QLabel;

    warningLbl = new QLabel;
    warningLbl->setObjectName("warning");

    // Grid rows and columns (in order of rows and columns)
    totalsLayout->addWidget(gdiv1,         0, 0, 1, 3, Qt::AlignVCenter);
    totalsLayout->addWidget(feeLbl,        1, 0,       Qt::AlignLeft | Qt::AlignVCenter);
    totalsLayout->addWidget(feeValueLbl,   1, 1,       Qt::AlignLeft | Qt::AlignVCenter);
    totalsLayout->addWidget(feeCol3,       1, 2        );
    totalsLayout->addWidget(gdiv2,         2, 0, 1, 3, Qt::AlignVCenter);
    totalsLayout->addWidget(totalLbl,      3, 0,       Qt::AlignLeft | Qt::AlignVCenter);
    totalsLayout->addWidget(totalValueLbl, 3, 1,       Qt::AlignLeft | Qt::AlignVCenter);
    totalsLayout->addWidget(totalCol3,     3, 2        );
    totalsLayout->addWidget(gdiv3,         4, 0, 1, 3, Qt::AlignVCenter);
    totalsLayout->addWidget(warningLbl,    5, 0, 1, 3, Qt::AlignLeft);

    totalsLayout->setColumnStretch(2, 1); // 3rd column
    totalsLayout->setRowMinimumHeight(0, BGU::spi(15)); // div 1
    totalsLayout->setRowMinimumHeight(1, BGU::spi(40)); // fee
    totalsLayout->setRowMinimumHeight(2, BGU::spi(15)); // div 2
    totalsLayout->setRowMinimumHeight(3, BGU::spi(40)); // total
    totalsLayout->setRowMinimumHeight(4, BGU::spi(15)); // div 3
    totalsLayout->setRowMinimumHeight(5, BGU::spi(30)); // warning

    confirmBtn = new BlocknetFormBtn;
    confirmBtn->setText(tr("Confirm Payment"));
    confirmBtn->setFocusPolicy(Qt::TabFocus);
    cancelBtn = new BlocknetFormBtn;
    cancelBtn->setObjectName("cancel");
    cancelBtn->setText(tr("Cancel"));

    auto *btnBox = new QFrame;
    auto *btnBoxLayout = new QHBoxLayout;
    btnBoxLayout->setContentsMargins(QMargins());
    btnBoxLayout->setSpacing(BGU::spi(15));
    btnBox->setLayout(btnBoxLayout);
    btnBoxLayout->addWidget(cancelBtn, 0, Qt::AlignLeft);
    btnBoxLayout->addWidget(confirmBtn, 0, Qt::AlignLeft);
    btnBoxLayout->addStretch(1);

    layout->addWidget(titleLbl, 0, Qt::AlignTop | Qt::AlignLeft);
    layout->addSpacing(BGU::spi(40));
    layout->addWidget(subtitleLbl, 0, Qt::AlignTop);
    layout->addSpacing(BGU::spi(20));
    layout->addWidget(addrBox, 0, Qt::AlignLeft);
    layout->addSpacing(BGU::spi(10));
    layout->addWidget(div1, 0);
    layout->addSpacing(BGU::spi(20));
    layout->addWidget(amountLbl, 0);
    layout->addSpacing(BGU::spi(20));
    layout->addWidget(amountBox, 0);
    layout->addSpacing(BGU::spi(20));
    layout->addWidget(totalGrid, 0);
    layout->addSpacing(BGU::spi(20));
    layout->addWidget(btnBox);
    layout->addStretch(1);

    connect(amountTi, &BlocknetLineEdit::textEdited, this, [this](const QString & text) {
        onAmountChanged();
    });
    connect(cancelBtn, &BlocknetFormBtn::clicked, this, &BlocknetQuickSend::onCancel);
    connect(confirmBtn, &BlocknetFormBtn::clicked, this, &BlocknetQuickSend::onSubmit);
    connect(addAddressBtn, &BlocknetIconBtn::clicked, this, &BlocknetQuickSend::openAddressBook);
}

bool BlocknetQuickSend::validated() {
    return walletModel->validateAddress(addressTi->text()) && !amountTi->text().isEmpty() &&
        BlocknetSendFundsModel::stringToInt(amountTi->text(), displayUnit) > 0;
}

void BlocknetQuickSend::keyPressEvent(QKeyEvent *event) {
    QWidget::keyPressEvent(event);
    if (this->isHidden())
        return;
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
        onSubmit();
}

bool BlocknetQuickSend::focusNextPrevChild(bool next) {
    if (next && amountTi->hasFocus()) {
        amountTi->clearFocus();
        this->setFocus(Qt::FocusReason::ActiveWindowFocusReason);
        return true;
    }
    return QFrame::focusNextPrevChild(next);
}

void BlocknetQuickSend::mouseReleaseEvent(QMouseEvent *event) {
    QWidget::mouseReleaseEvent(event);
    if (this->amountTi->hasFocus() && !this->amountTi->rect().contains(event->pos())) {
        this->amountTi->clearFocus();
        this->setFocus(Qt::FocusReason::ActiveWindowFocusReason);
    }
}

void BlocknetQuickSend::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    if (!addressTi->hasFocus() && !amountTi->hasFocus())
        addressTi->setFocus(Qt::FocusReason::ActiveWindowFocusReason);
    connect(walletModel, &WalletModel::encryptionStatusChanged, this, &BlocknetQuickSend::onEncryptionStatus);
    connect(walletModel->getOptionsModel(), &OptionsModel::displayUnitChanged, this, &BlocknetQuickSend::onDisplayUnit);
}

void BlocknetQuickSend::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    disconnect(walletModel, &WalletModel::encryptionStatusChanged, this, &BlocknetQuickSend::onEncryptionStatus);
    disconnect(walletModel->getOptionsModel(), &OptionsModel::displayUnitChanged, this, &BlocknetQuickSend::onDisplayUnit);
}

void BlocknetQuickSend::addAddress(const QString &address) {
    addressTi->setText(address);
}

void BlocknetQuickSend::openAddressBook() {
    BlocknetAddressBookDialog dlg(walletModel, Qt::WindowSystemMenuHint | Qt::WindowTitleHint);
    dlg.singleShotMode();
    connect(&dlg, &BlocknetAddressBookDialog::send, [this](const QString &address) {
        addAddress(address);
    });
    dlg.exec();
}

void BlocknetQuickSend::onAmountChanged() {
    BlocknetSendFundsModel model;
    const auto addrLabel = walletModel->getAddressTableModel()->labelForAddress(addressTi->text());
    model.addRecipient(addressTi->text(), BlocknetSendFundsModel::stringToInt(amountTi->text(), displayUnit), addrLabel);

    CCoinControl cc = model.getCoinControl(walletModel);
    if (!walletModel->wallet().isLocked()) { // if the wallet is unlocked, calculate exact fees
        auto status = model.prepareFunds(walletModel, cc);
        QString feeAmount = BitcoinUnits::formatWithUnit(displayUnit, model.txFees());
        feeValueLbl->setText(feeAmount);
        // Display or clear the warning message according to the wallet status
        if (status.status != WalletModel::OK)
            warningLbl->setText(BlocknetSendFundsPage::processSendCoinsReturn(walletModel, status).first);
        else warningLbl->clear();
    } else if (cc.HasSelected()) { // estimate b/c wallet is locked here
        const auto feeInfo = BlocknetEstimateFee(walletModel, cc, model.subtractFee(), model.txRecipients());
        QString feeAmount = BitcoinUnits::formatWithUnit(displayUnit, std::get<0>(feeInfo));
        feeValueLbl->setText(QString("%1 %2").arg(feeAmount, tr("(estimated)")));
        model.setEstimatedFees(std::get<0>(feeInfo));
    } else { // estimate b/c wallet is locked here
        cc.m_feerate.reset(); // explicitly use only fee estimation rate for smart fee labels
        int estimatedConfirmations;
        FeeReason reason;
        CFeeRate feeRate = CFeeRate(walletModel->wallet().getMinimumFee(1000, cc, &estimatedConfirmations, &reason));
        QString feeAmount = BitcoinUnits::formatWithUnit(displayUnit, feeRate.GetFeePerK()) + "/kB";
        feeValueLbl->setText(QString("%1 %2").arg(feeAmount, tr("(estimated)")));
        model.setEstimatedFees(feeRate.GetFeePerK());
    }

    CAmount fees = walletModel->wallet().isLocked() ? model.estimatedFees() : model.txFees();
    if (model.subtractFee())
        fees *= -1;
    totalValueLbl->setText(BitcoinUnits::formatWithUnit(displayUnit, (walletModel->wallet().isLocked() ? fees : 0) + model.txTotalAmount()));
}

void BlocknetQuickSend::onSubmit() {
    if (!this->validated()) {
        QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Please specify a send address and an amount larger than %1")
                .arg(BitcoinUnits::formatWithUnit(displayUnit, ::minRelayTxFee.GetFeePerK())));
        return;
    }

    // Unlock wallet context (for relock)
    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();
    if (encStatus == WalletModel::EncryptionStatus::Locked || util::unlockedForStakingOnly) {
        const bool stateUnlockForStaking = util::unlockedForStakingOnly;
        WalletModel::UnlockContext ctx(walletModel->requestUnlock());
        if (!ctx.isValid() || util::unlockedForStakingOnly) {
            QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Failed to unlock the wallet"));
        } else {
            submitFunds();
            util::unlockedForStakingOnly = stateUnlockForStaking; // restore unlocked for staking state
        }
        return;
    }

    submitFunds();
}

WalletModel::SendCoinsReturn BlocknetQuickSend::submitFunds() {
    QList<SendCoinsRecipient> recipients;
    const auto addrLabel = walletModel->getAddressTableModel()->labelForAddress(addressTi->text());
    recipients.push_back(SendCoinsRecipient{ addressTi->text(), addrLabel,
                                             BlocknetSendFundsModel::stringToInt(amountTi->text(), displayUnit),
                                             QString() });
    WalletModelTransaction currentTransaction(recipients);
    CCoinControl ctrl;
    WalletModel::SendCoinsReturn prepareStatus = walletModel->prepareTransaction(currentTransaction, ctrl);
    const auto feeMsg = BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(),
                                                     currentTransaction.getTransactionFee());
    if (prepareStatus.status != WalletModel::OK) {
        // process prepareStatus and on error generate message shown to user
        auto res = BlocknetSendFundsPage::processSendCoinsReturn(walletModel, prepareStatus, feeMsg);
        updateLabels(prepareStatus);
        if (res.second)
            QMessageBox::critical(this->parentWidget(), tr("Issue"), res.first);
        else
            QMessageBox::warning(this->parentWidget(), tr("Issue"), res.first);
        return prepareStatus;
    }

    const auto txFee = currentTransaction.getTransactionFee();
    const auto totalAmt = currentTransaction.getTotalTransactionAmount() + txFee;

    // Update labels
    feeValueLbl->setText(BitcoinUnits::formatWithUnit(displayUnit, txFee));
    totalValueLbl->setText(BitcoinUnits::formatWithUnit(displayUnit, totalAmt));

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
        questionString.append(" (" + QString::number((double)currentTransaction.getTransactionSize() / 1000) + " kB): ");
        // append transaction fee value
        questionString.append("<span style='color:#aa0000; font-weight:bold;'>");
        questionString.append(BitcoinUnits::formatHtmlWithUnit(displayUnit, txFee));
        questionString.append("</span><br />");
    }
    // add total amount in all subdivision units
    questionString.append("<hr />");
    questionString.append(QString("<b>%1</b>: <b>%2</b>").arg(tr("Total Amount"))
                                  .arg(BitcoinUnits::formatHtmlWithUnit(displayUnit, totalAmt)));
    SendConfirmationDialog confirmationDialog(tr("Confirm send coins"),
                                              questionString.arg(formatted.join("<br />")), SEND_CONFIRM_DELAY, this);
    confirmationDialog.exec();

    WalletModel::SendCoinsReturn sendStatus(WalletModel::StatusCode::TransactionCommitFailed);
    bool canceledSend{false};
    auto retval = static_cast<QMessageBox::StandardButton>(confirmationDialog.result());
    if (retval == QMessageBox::Yes) {
        // now send the prepared transaction
        sendStatus = walletModel->sendCoins(currentTransaction);
        // process sendStatus and on error generate message shown to user
        auto res = BlocknetSendFundsPage::processSendCoinsReturn(walletModel, sendStatus, feeMsg);
        if (sendStatus.status == WalletModel::OK)
            Q_EMIT submit();
        else {
            updateLabels(sendStatus);
            if (res.second)
                QMessageBox::critical(this->parentWidget(), tr("Issue"), res.first);
            else
                QMessageBox::warning(this->parentWidget(), tr("Issue"), res.first);
        }
    } else
        canceledSend = true;

    // Display warning message if necessary
    if (!canceledSend && sendStatus.status != WalletModel::OK && warningLbl)
        warningLbl->setText(BlocknetSendFundsRequest::sendStatusMsg(sendStatus, "", displayUnit));

    return sendStatus;
}

void BlocknetQuickSend::onEncryptionStatus() {
    if (walletModel->getEncryptionStatus() == WalletModel::Unlocked && !addressTi->text().isEmpty())
        onAmountChanged();
}

void BlocknetQuickSend::onDisplayUnit(int unit) {
    displayUnit = unit;
    amountTi->setValidator(new BlocknetNumberValidator(0, BLOCKNETGUI_FUNDS_MAX, BitcoinUnits::decimals(displayUnit)));
    onAmountChanged();
}

void BlocknetQuickSend::updateLabels(const WalletModel::SendCoinsReturn & result) {
    // Handle errors
    if (result.status != WalletModel::OK)
        warningLbl->setText(BlocknetSendFundsRequest::sendStatusMsg(result, "", displayUnit));
    else warningLbl->clear();
    feeValueLbl->setText(QString("%1 %2").arg(BitcoinUnits::formatWithUnit(displayUnit, txFees), tr("(estimated)")));
    totalValueLbl->setText(BitcoinUnits::formatWithUnit(displayUnit, totalAmount));
}
