// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetsendfunds4.h"
#include "blocknethdiv.h"
#include "blocknetcircle.h"
#include "blocknetsendfundsrequest.h"

#include "optionsmodel.h"

#include <QMessageBox>
#include <QKeyEvent>
#include <QDebug>
#include <utility>

BlocknetSendFunds4::BlocknetSendFunds4(WalletModel *w, int id, QFrame *parent) : BlocknetSendFundsPage(w, id, parent),
                                                                                 layout(new QVBoxLayout) {
//    this->setStyleSheet("border: 1px solid red");
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(layout);
    layout->setContentsMargins(15, 10, 35, 30);

    titleLbl = new QLabel(tr("Send Funds"));
    titleLbl->setObjectName("h4");

    QLabel *subtitleLbl = new QLabel(tr("Review Payment"));
    subtitleLbl->setObjectName("h2");

    auto *btnBox = new QFrame;
    auto *btnBoxLayout = new QHBoxLayout;
    btnBoxLayout->setContentsMargins(QMargins());
    btnBoxLayout->setSpacing(15);
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
    totalsLayout->setContentsMargins(0, 0, 35, 0);
    totalsLayout->setSpacing(50);
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

    totalsLayout->addWidget(div1, 0, 0, 1, 3); totalsLayout->setRowMinimumHeight(0, 15);
    totalsLayout->addWidget(feeLbl, 1, 0, Qt::AlignLeft | Qt::AlignVCenter);
    totalsLayout->addWidget(feeValueLbl, 1, 1, Qt::AlignLeft | Qt::AlignVCenter);
    totalsLayout->addWidget(pl1, 1, 2, Qt::AlignRight);
    totalsLayout->addWidget(div2, 2, 0, 1, 3); totalsLayout->setRowMinimumHeight(2, 15);
    totalsLayout->addWidget(totalLbl, 3, 0, Qt::AlignLeft | Qt::AlignVCenter);
    totalsLayout->addWidget(totalValueLbl, 3, 1, Qt::AlignLeft | Qt::AlignVCenter);
    totalsLayout->addWidget(pl2, 3, 2, Qt::AlignRight);
    totalsLayout->setColumnStretch(1, 1);
    totalsLayout->setColumnStretch(2, 1);
    totalsLayout->setRowMinimumHeight(1, 45);
    totalsLayout->setRowMinimumHeight(3, 45);
    // End Totals box

    warningLbl = new QLabel;
    warningLbl->setObjectName("warning");

    layout->addWidget(titleLbl, 0, Qt::AlignTop | Qt::AlignLeft);
    layout->addSpacing(45);
    layout->addWidget(subtitleLbl, 0, Qt::AlignTop);
    layout->addSpacing(30);
    layout->addWidget(content, 10);
    layout->addSpacing(10);
    layout->addWidget(totals);
    layout->addSpacing(10);
    layout->addWidget(warningLbl);
    layout->addSpacing(10);
    layout->addWidget(btnBox);

    connect(continueBtn, SIGNAL(clicked()), this, SLOT(onSubmit()));
    connect(cancelBtn, SIGNAL(clicked()), this, SLOT(onCancel()));
    connect(backBtn, SIGNAL(clicked()), this, SLOT(onBack()));
}

/**
 * @brief Builds the summary list.
 * @param data
 */
void BlocknetSendFunds4::setData(BlocknetSendFundsModel *model) {
    BlocknetSendFundsPage::setData(model);
    displayUnit = walletModel->getOptionsModel()->getDisplayUnit();
    displayMultiple();
    fillWalletData();
}

void BlocknetSendFunds4::onEncryptionStatus(int encStatus) {
    if (!this->isHidden() && encStatus == WalletModel::Unlocked) {
        auto coinControl = model->getCoinControl(walletModel);
        model->prepareFunds(walletModel, &coinControl);
        fillWalletData();
    }
}

/**
 * @brief Asks the wallet to create a transaction in order to obtain more accurate fee information.
 */
void BlocknetSendFunds4::fillWalletData() {
    // Disable the confirm button if there are any problems
    continueBtn->setDisabled(model->txStatus.status != WalletModel::OK);

    if (model->txStatus.status != WalletModel::OK && model->txStatus.status != WalletModel::Cancel) {
        // Handle errors
        feeValueLbl->setText(tr("n/a"));
        totalValueLbl->setText(tr("n/a"));
        auto status = model->txStatus;
        warningLbl->setText(walletModel->messageForSendCoinsReturn(status));
        return;
    }

    feeValueLbl->setText(feeText(BitcoinUnits::formatWithUnit(displayUnit, model->txActiveFee())));
    totalValueLbl->setText(totalText(BitcoinUnits::formatWithUnit(displayUnit, model->txTotalAmount())));
    warningLbl->clear();
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
    if (model->recipients.isEmpty()) {
        auto msg = tr("Please add recipients.");
        QMessageBox::warning(this->parentWidget(), tr("Issue"), msg);
        return false;
    }

    auto list = model->recipients.toList();
    bool allValid = [](QList<BlocknetTransaction> &recipients, WalletModel *w) -> bool {
        for (BlocknetTransaction &rec : recipients) {
            if (!rec.isValid(w))
                return false;
        }
        return true;
    }(list, walletModel);
    if (list.isEmpty() || !allValid) {
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
    connect(walletModel, SIGNAL(encryptionStatusChanged(int)), this, SLOT(onEncryptionStatus(int)));
    connect(walletModel->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(onDisplayUnit(int)));
}

void BlocknetSendFunds4::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    disconnect(walletModel, SIGNAL(encryptionStatusChanged(int)), this, SLOT(onEncryptionStatus(int)));
    disconnect(walletModel->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(onDisplayUnit(int)));
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
    scrollc->setContentsMargins(0, 0, 20, 0);
    scrollc->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum);
    auto *scrollcLayout = new QGridLayout;
    scrollcLayout->setContentsMargins(QMargins());
    scrollcLayout->setSpacing(50);
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
        scrollcLayout->setRowMinimumHeight(0, 40);
    }

    // Add empty transaction struct to simulate recipients dividers
    QVector<BlocknetTransaction> drawRecipients;
    QList<BlocknetTransaction> recs;
    if (!model->recipients.isEmpty())
        recs = model->recipients.toList();
    for (int j = 0; j < recs.count(); ++j) {
        drawRecipients << BlocknetTransaction();
        drawRecipients << recs[j];
    }

    // Display summaries of all addresses (in recipients)
    int i;
    for (i = 0; i < drawRecipients.count(); ++i) {
        int row = i + 1; // account for header row

        auto recipient = drawRecipients[i];

        if (recipient.address.isEmpty()) {
            auto *div = new BlocknetHDiv;
            scrollcLayout->addWidget(div, row, 0, 1, 3, Qt::AlignVCenter);
            scrollcLayout->setRowMinimumHeight(row, 15);
            continue;
        }

        auto *box = new QFrame;
        box->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
        auto *boxLayout = new QHBoxLayout;
        boxLayout->setContentsMargins(QMargins());
        box->setLayout(boxLayout);
        auto *addrLbl = new QLabel(recipient.address);
        addrLbl->setObjectName("standard");
        boxLayout->addWidget(new BlocknetCircle(35, 35), 0, Qt::AlignVCenter);
        boxLayout->addWidget(addrLbl, 1);
        scrollcLayout->addWidget(box, row, 0, Qt::AlignLeft | Qt::AlignVCenter); scrollcLayout->setColumnStretch(0, 0);

        auto *amountLbl = new QLabel();
        amountLbl->setObjectName("standard");
        amountLbl->setText(recipient.getAmountAfterFee(displayUnit));
        scrollcLayout->addWidget(amountLbl, row, 1, Qt::AlignLeft | Qt::AlignVCenter); scrollcLayout->setColumnStretch(0, 1);

        auto *edit = new QPushButton(tr("Edit"));
        edit->setObjectName("linkBtn");
        edit->setCursor(Qt::PointingHandCursor);
        connect(edit, SIGNAL(clicked()), this, SLOT(onEdit()));
        scrollcLayout->addWidget(edit, row, 10, Qt::AlignRight | Qt::AlignVCenter);  scrollcLayout->setColumnStretch(0, 2);

        scrollcLayout->setRowMinimumHeight(row, 40);
    }
    auto *spacer = new QFrame;
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    scrollcLayout->addWidget(spacer, i + 1, 0, 1, 3);

    recipientsLayout->addWidget(scrollArea, 10);
    contentLayout->addWidget(recipients, 1);

    feeValueLbl->setText(feeText(BitcoinUnits::formatWithUnit(displayUnit, model->txActiveFee())));
    totalValueLbl->setText(totalText(BitcoinUnits::formatWithUnit(displayUnit, model->txTotalAmount())));
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
    emit edit();
}

void BlocknetSendFunds4::onSubmit() {
    if (!this->validated())
        return;

    // Use lambda to encapsulate the send request in a wallet unlock context
    auto send = [this]() {
        bool zeroFee = model->customFee && model->userFee == 0;
        if (zeroFee)
            walletModel->attemptToSendZeroFee();

        if (warningLbl) warningLbl->clear();

        CAmount fees{0};
        CAmount amount{0};
        bool unlocked = false;
        auto coinControl = model->getCoinControl(walletModel);
        auto *sendFundsRequest = new BlocknetSendFundsRequest(this, walletModel, &coinControl);
        auto result = sendFundsRequest->send(model->txRecipients, fees, amount, unlocked);

        model->txFees = fees;
        model->txAmount = amount;

        if (result.status != WalletModel::OK && result.status != WalletModel::Cancel) {
            if (feeValueLbl) feeValueLbl->setText(tr("n/a"));
            if (totalValueLbl) totalValueLbl->setText(tr("n/a"));
            if (warningLbl) warningLbl->setText(walletModel->messageForSendCoinsReturn(result));
            return;
        }

        displayMultiple();
        fillWalletData();

        if (result.status == WalletModel::OK)
            emit submit();

        if (zeroFee) // Unset
            walletModel->attemptToSendZeroFee(false);
    };

    // Unlock wallet context (for relock)
    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();
    if (encStatus == walletModel->Locked || encStatus == walletModel->UnlockedForAnonymizationOnly) {
        WalletModel::UnlockContext ctx(walletModel->requestUnlock(true));
        if (!ctx.isValid()) {// Unlock wallet was cancelled
            if (warningLbl) warningLbl->setText(tr("Wallet unlock failed, please try again"));
        } else send();
        return;
    }

    // wallet is already unlocked
    send();
}

void BlocknetSendFunds4::onDisplayUnit(int unit) {
    displayUnit = unit;
    auto coinControl = model->getCoinControl(walletModel);
    model->prepareFunds(walletModel, &coinControl);
    displayMultiple();
    fillWalletData();
}

QString BlocknetSendFunds4::feeText(QString fee) {
    if (model->isZeroFee())
        return QString("%1 %2").arg(fee, tr("(attempting zero fee)"));
    else if (!model->customFee) {
        // The fee is an estimate if the wallet is locked
        WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();
        bool estimatingFee = encStatus == walletModel->Locked || encStatus == walletModel->UnlockedForAnonymizationOnly;
        QString f = (estimatingFee ? tr("(estimated since wallet is locked)") : "");
        if (estimatingFee)
            return QString("%1 %2").arg(fee, f);
    }
    return fee;
}

QString BlocknetSendFunds4::totalText(QString total) {
    if (model->subtractFee)
        return QString("%1 %2").arg(total, tr("(including fee)"));
    return total;
}
