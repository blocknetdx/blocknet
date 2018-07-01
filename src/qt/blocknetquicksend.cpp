// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetquicksend.h"
#include "blocknethdiv.h"
#include "blocknetsendfundsrequest.h"

#include "optionsmodel.h"
#include "amount.h"
#include "coincontrol.h"
#include "base58.h"

#include <QMessageBox>
#include <QDoubleValidator>
#include <QKeyEvent>

BlocknetQuickSend::BlocknetQuickSend(WalletModel *w, QFrame *parent) : QFrame(parent), walletModel(w),
                                                                       layout(new QVBoxLayout) {
//    this->setStyleSheet("border: 1px solid red");
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(layout);
    layout->setContentsMargins(45, 10, 45, 30);

    displayUnit = walletModel->getOptionsModel()->getDisplayUnit();
    auto displayUnitName = BitcoinUnits::name(displayUnit);

    titleLbl = new QLabel(tr("Quick Send"));
    titleLbl->setObjectName("h4");

    QLabel *subtitleLbl = new QLabel(tr("Who would you like to send funds to?"));
    subtitleLbl->setObjectName("h2");

    addressTi = new BlocknetLineEdit;
    addressTi->setObjectName("address");
    addressTi->setPlaceholderText(tr("Enter Blocknet Address..."));
    addressTi->setFocusPolicy(Qt::FocusPolicy::StrongFocus);
    addressTi->setValidator(new QRegExpValidator(QRegExp("[a-zA-Z0-9]{33,35}"), this));

    QLabel *amountLbl = new QLabel(tr("How much would you like to send?"));
    amountLbl->setObjectName("h2");
    auto *amountBox = new QFrame;
    auto *amountBoxLayout = new QHBoxLayout;
    amountBoxLayout->setContentsMargins(QMargins());
    amountBox->setLayout(amountBoxLayout);
    amountTi = new BlocknetLineEdit;
    amountTi->setPlaceholderText(tr("Enter Amount..."));
    amountTi->setValidator(new QDoubleValidator(0, BLOCKNETGUI_FUNDS_MAX, BitcoinUnits::decimals(displayUnit)));
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
    totalsLayout->setSpacing(50);
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
    totalsLayout->setRowMinimumHeight(0, 15); // div 1
    totalsLayout->setRowMinimumHeight(1, 40); // fee
    totalsLayout->setRowMinimumHeight(2, 15); // div 2
    totalsLayout->setRowMinimumHeight(3, 40); // total
    totalsLayout->setRowMinimumHeight(4, 15); // div 3
    totalsLayout->setRowMinimumHeight(5, 30); // warning

    confirmBtn = new BlocknetFormBtn;
    confirmBtn->setText(tr("Confirm Payment"));
    cancelBtn = new BlocknetFormBtn;
    cancelBtn->setObjectName("cancel");
    cancelBtn->setText(tr("Cancel"));

    auto *btnBox = new QFrame;
    auto *btnBoxLayout = new QHBoxLayout;
    btnBoxLayout->setContentsMargins(QMargins());
    btnBoxLayout->setSpacing(15);
    btnBox->setLayout(btnBoxLayout);
    btnBoxLayout->addWidget(cancelBtn, 0, Qt::AlignLeft);
    btnBoxLayout->addWidget(confirmBtn, 0, Qt::AlignLeft);
    btnBoxLayout->addStretch(1);

    layout->addWidget(titleLbl, 0, Qt::AlignTop | Qt::AlignLeft);
    layout->addSpacing(40);
    layout->addWidget(subtitleLbl, 0, Qt::AlignTop);
    layout->addSpacing(20);
    layout->addWidget(addressTi, 0);
    layout->addSpacing(40);
    layout->addWidget(div1, 0);
    layout->addSpacing(35);
    layout->addWidget(amountLbl, 0);
    layout->addSpacing(20);
    layout->addWidget(amountBox, 0);
    layout->addSpacing(40);
    layout->addWidget(totalGrid, 0);
    layout->addSpacing(40);
    layout->addWidget(btnBox);
    layout->addStretch(1);

    connect(amountTi, SIGNAL(textChanged(const QString&)), this, SLOT(onAmountChanged(const QString&)));
    connect(cancelBtn, SIGNAL(clicked()), this, SLOT(onCancel()));
    connect(confirmBtn, SIGNAL(clicked()), this, SLOT(onSubmit()));

    onAmountChanged();
}

bool BlocknetQuickSend::validated() {
    return walletModel->validateAddress(addressTi->text()) && !amountTi->text().isEmpty() &&
           BlocknetTransaction::stringToInt(amountTi->text(), displayUnit) > 0;
}

void BlocknetQuickSend::keyPressEvent(QKeyEvent *event) {
    QWidget::keyPressEvent(event);
    if (this->isHidden())
        return;
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
        onSubmit();
}

void BlocknetQuickSend::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    if (!addressTi->hasFocus() && !amountTi->hasFocus())
        addressTi->setFocus(Qt::FocusReason::ActiveWindowFocusReason);
    connect(walletModel, SIGNAL(encryptionStatusChanged(int)), this, SLOT(onEncryptionStatus(int)));
    connect(walletModel->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(onDisplayUnit(int)));
}

void BlocknetQuickSend::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    disconnect(walletModel, SIGNAL(encryptionStatusChanged(int)), this, SLOT(onEncryptionStatus(int)));
    disconnect(walletModel->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(onDisplayUnit(int)));
}

void BlocknetQuickSend::onAmountChanged(const QString &text) {
    if (!validated()) {
        CAmount fees = walletModel->estimatedFee(1).GetFeePerK();
        feeValueLbl->setText(QString("%1/kB %2").arg(BitcoinUnits::formatWithUnit(displayUnit, fees), tr("(estimated)")));
        totalValueLbl->setText(BitcoinUnits::formatWithUnit(displayUnit, BlocknetTransaction::stringToInt(amountTi->text(), displayUnit) + fees));
        return;
    }
    // Do not process unless the amounts changed
    CAmount cur = BlocknetTransaction::stringToInt(amountTi->text(), displayUnit);
    if (lastAmount == cur)
        return;
    lastAmount = cur;

    auto result = processFunds();
    if (result.status != WalletModel::OK && result.status != WalletModel::Cancel) {
        // Handle errors
        feeValueLbl->setText(tr("n/a"));
        totalValueLbl->setText(tr("n/a"));
        warningLbl->setText(walletModel->messageForSendCoinsReturn(result));
        return;
    }

    feeValueLbl->setText(BitcoinUnits::formatWithUnit(displayUnit, txFees));
    totalValueLbl->setText(BitcoinUnits::formatWithUnit(displayUnit, totalAmount));
    warningLbl->clear();
}

void BlocknetQuickSend::onSubmit() {
    if (!this->validated()) {
        QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Please specify a send address and an amount larger than %1.")
                .arg(BitcoinUnits::formatWithUnit(displayUnit, ::minRelayTxFee.GetFeePerK())));
        return;
    }

    auto result = processFunds(true);
    if (result.status != WalletModel::OK && result.status != WalletModel::Cancel) {
        // Handle errors
        feeValueLbl->setText(tr("n/a"));
        totalValueLbl->setText(tr("n/a"));
        warningLbl->setText(walletModel->messageForSendCoinsReturn(result));
        return;
    }

    warningLbl->clear();
}

WalletModel::SendCoinsReturn BlocknetQuickSend::processFunds(bool submitFunds) {
    BlocknetSendFundsModel model;
    model.addRecipient(addressTi->text(), BlocknetTransaction::stringToInt(amountTi->text(), displayUnit));
    model.userFee = walletModel->estimatedFee(1).GetFeePerK();
    model.customFee = false;

    QList<SendCoinsRecipient> recipients;

    for (const BlocknetTransaction &tx : model.recipients) {
        SendCoinsRecipient recipient;
        recipient.address = tx.address;
        recipient.amount = tx.amount;
        recipient.useSwiftTX = false;
        recipient.inputType = ALL_COINS;
        recipients << recipient;
    }

    if (recipients.isEmpty())
        return WalletModel::InvalidAddress;

    // Coin control options
    CCoinControl coinControl;
    if (walletModel->validateAddress(model.changeAddress)) {
        CBitcoinAddress addr(model.changeAddress.toStdString());
        if (addr.IsValid())
            coinControl.destChange     = addr.Get();
    }
    coinControl.fAllowOtherInputs      = true;
    coinControl.fOverrideFeeRate       = model.customFee;
    if (model.customFee)
        coinControl.nFeeRate           = CFeeRate(model.userFee);
    coinControl.fAllowZeroValueOutputs = false;

    WalletModel::SendCoinsReturn result;

    if (submitFunds) {
        auto *sendFundsRequest = new BlocknetSendFundsRequest(this, walletModel, &coinControl);
        result = sendFundsRequest->send(recipients, txFees, txAmount);
        if (result.status == WalletModel::OK) {
            totalAmount = txAmount + txFees;
            emit submit();
            return result;
        } else {
            txFees = 0;
            txAmount = 0;
            totalAmount = 0;
        }
        return result;
    }

    WalletModelTransaction walletTx(recipients);

    if (walletModel->isWalletLocked())
        result = walletModel->prepareTransaction(walletTx, &coinControl, 0, false);
    else
        result = walletModel->prepareTransaction(walletTx, &coinControl);

    if (result.status == WalletModel::OK || result.status == WalletModel::Cancel) {
        txFees = walletTx.getTransactionFee();
        txAmount = walletTx.getTotalTransactionAmount();
        totalAmount = txAmount + txFees;
    } else {
        txFees = 0;
        txAmount = 0;
        totalAmount = 0;
    }

    return result;
}

void BlocknetQuickSend::onEncryptionStatus(int encStatus) {
    if (encStatus == WalletModel::Unlocked)
        onAmountChanged();
}

void BlocknetQuickSend::onDisplayUnit(int unit) {
    displayUnit = unit;
    amountTi->setValidator(new QDoubleValidator(0, BLOCKNETGUI_FUNDS_MAX, BitcoinUnits::decimals(displayUnit)));
    onAmountChanged();
}
