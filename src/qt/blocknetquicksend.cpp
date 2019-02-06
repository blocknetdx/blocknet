// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetquicksend.h"
#include "blocknetsendfundsrequest.h"
#include "blocknetsendfundsutil.h"
#include "blocknetaddressbook.h"
#include "blocknethdiv.h"
#include "blockneticonbtn.h"

#include "optionsmodel.h"
#include "amount.h"
#include "coincontrol.h"
#include "base58.h"

#include <QMessageBox>
#include <QLineEdit>
#include <QKeyEvent>

BlocknetQuickSend::BlocknetQuickSend(WalletModel *w, QWidget *parent) : QFrame(parent), walletModel(w),
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

    auto *addrBox = new QFrame;
    addrBox->setContentsMargins(QMargins());
    auto *addrBoxLayout = new QHBoxLayout;
    addrBoxLayout->setContentsMargins(QMargins());
    addrBox->setLayout(addrBoxLayout);
    addressTi = new BlocknetLineEdit(400);
    addressTi->setObjectName("address");
    addressTi->setPlaceholderText(tr("Enter Blocknet Address..."));
    addressTi->setFocusPolicy(Qt::FocusPolicy::StrongFocus);
    addressTi->setValidator(new QRegExpValidator(QRegExp("[a-zA-Z0-9]{33,35}"), this));
    auto *addAddressBtn = new BlocknetIconBtn(QString(), ":/redesign/QuickActions/AddressBookIcon.png");
    addrBoxLayout->addWidget(addressTi, 0, Qt::AlignTop);
    addrBoxLayout->addSpacing(20);
    addrBoxLayout->addWidget(addAddressBtn, 0, Qt::AlignTop);

    QLabel *amountLbl = new QLabel(tr("How much would you like to send?"));
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
    confirmBtn->setFocusPolicy(Qt::TabFocus);
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
    layout->addWidget(addrBox, 0, Qt::AlignLeft);
    layout->addSpacing(10);
    layout->addWidget(div1, 0);
    layout->addSpacing(20);
    layout->addWidget(amountLbl, 0);
    layout->addSpacing(20);
    layout->addWidget(amountBox, 0);
    layout->addSpacing(20);
    layout->addWidget(totalGrid, 0);
    layout->addSpacing(20);
    layout->addWidget(btnBox);
    layout->addStretch(1);

    connect(amountTi, &BlocknetLineEdit::editingFinished, this, &BlocknetQuickSend::onAmountChanged);
    connect(cancelBtn, SIGNAL(clicked()), this, SLOT(onCancel()));
    connect(confirmBtn, SIGNAL(clicked()), this, SLOT(onSubmit()));
    connect(addAddressBtn, SIGNAL(clicked()), this, SLOT(openAddressBook()));

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
    connect(walletModel, SIGNAL(encryptionStatusChanged(int)), this, SLOT(onEncryptionStatus(int)));
    connect(walletModel->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(onDisplayUnit(int)));
}

void BlocknetQuickSend::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    disconnect(walletModel, SIGNAL(encryptionStatusChanged(int)), this, SLOT(onEncryptionStatus(int)));
    disconnect(walletModel->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(onDisplayUnit(int)));
}

void BlocknetQuickSend::addAddress(const QString &address) {
    addressTi->setText(address);
}

void BlocknetQuickSend::openAddressBook() {
    BlocknetAddressBookDialog dlg(walletModel, Qt::WindowSystemMenuHint | Qt::WindowTitleHint);
    dlg.singleShotMode();
    connect(&dlg, &BlocknetAddressBookDialog::send, this, [this](const QString &address) {
        addAddress(address);
    });
    dlg.exec();
}

void BlocknetQuickSend::onAmountChanged() {
    if (!validated()) {
        bool feesKnown = txFees > 0;
        if (!feesKnown) {
            CAmount fees = walletModel->estimatedFee(1).GetFeePerK();
            feeValueLbl->setText(QString("%1/kB %2").arg(BitcoinUnits::formatWithUnit(displayUnit, fees), tr("(estimated)")));
            totalValueLbl->setText(BitcoinUnits::formatWithUnit(displayUnit, BlocknetTransaction::stringToInt(amountTi->text(), displayUnit) + fees));
            return;
        }
        feeValueLbl->setText(QString("%1 %2").arg(BitcoinUnits::formatWithUnit(displayUnit, txFees), walletModel->isWalletLocked() ? tr("(estimated)") : QString()));
        totalValueLbl->setText(BitcoinUnits::formatWithUnit(displayUnit, BlocknetTransaction::stringToInt(amountTi->text(), displayUnit) + txFees));
        return;
    }
    // Do not process unless the amounts changed
    CAmount cur = BlocknetTransaction::stringToInt(amountTi->text(), displayUnit);
    if (lastAmount == cur)
        return;
    lastAmount = cur;

    processFunds();
}

void BlocknetQuickSend::onSubmit() {
    if (!this->validated()) {
        QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Please specify a send address and an amount larger than %1.")
                .arg(BitcoinUnits::formatWithUnit(displayUnit, ::minRelayTxFee.GetFeePerK())));
        return;
    }

    processFunds(true);
}

WalletModel::SendCoinsReturn BlocknetQuickSend::processFunds(bool submitFunds) {
    walletUnlockedFee = false;

    BlocknetSendFundsModel model;
    model.addRecipient(addressTi->text(), BlocknetTransaction::stringToInt(amountTi->text(), displayUnit));
    model.userFee = walletModel->estimatedFee(1).GetFeePerK();
    model.customFee = false;

    WalletModel::SendCoinsReturn result;
    walletModel->attemptToSendZeroFee();

    // Coin control options
    auto coinControl = model.getCoinControl(walletModel);

    // calculate the fees
    result = model.prepareFunds(walletModel, &coinControl);
    txFees = model.txActiveFee();
    txAmount = model.txAmount;
    totalAmount = model.txTotalAmount();
    if (result.status != WalletModel::OK && result.status != WalletModel::Cancel) {
        walletModel->attemptToSendZeroFee(false);
        updateLabels(result);
        return result;
    }

    // Submit the transaction
    if (submitFunds) {
        CAmount fees{0};
        CAmount amount{0};
        bool unlocked = false;
        auto *sendFundsRequest = new BlocknetSendFundsRequest(this, walletModel, &coinControl);
        result = sendFundsRequest->send(model.txRecipients, fees, amount, unlocked);
        if (unlocked) {
            model.txFees = fees;
            model.txAmount = amount;
            totalAmount = model.txTotalAmount();
            txFees = model.txActiveFee();
            txAmount = model.txAmount;
            walletUnlockedFee = true;
        }

        if (result.status == WalletModel::OK)
            emit submit();
    }

    walletModel->attemptToSendZeroFee(false);
    updateLabels(result);

    return result;
}

void BlocknetQuickSend::onEncryptionStatus(int encStatus) {
    if (encStatus == WalletModel::Unlocked)
        onAmountChanged();
}

void BlocknetQuickSend::onDisplayUnit(int unit) {
    displayUnit = unit;
    amountTi->setValidator(new BlocknetNumberValidator(0, BLOCKNETGUI_FUNDS_MAX, BitcoinUnits::decimals(displayUnit)));
    onAmountChanged();
}

void BlocknetQuickSend::updateLabels(WalletModel::SendCoinsReturn &result) {
    // Handle errors
    if (result.status != WalletModel::OK && result.status != WalletModel::Cancel) {
        warningLbl->setText(walletModel->messageForSendCoinsReturn(result));
    } else {
        warningLbl->clear();
    }

    feeValueLbl->setText(QString("%1 %2").arg(BitcoinUnits::formatWithUnit(displayUnit, txFees), walletModel->isWalletLocked()
                                                           && !walletUnlockedFee ? tr("(estimated since wallet is locked)") : ""));
    totalValueLbl->setText(BitcoinUnits::formatWithUnit(displayUnit, totalAmount));
}
