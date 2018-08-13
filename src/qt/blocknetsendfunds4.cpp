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
    layout->setContentsMargins(15, 10, 0, 30);

    titleLbl = new QLabel(tr("Send Funds"));
    titleLbl->setObjectName("h4");

    QLabel *subtitleLbl = new QLabel(tr("Review Payment"));
    subtitleLbl->setObjectName("h2");

    continueBtn = new BlocknetFormBtn;
    continueBtn->setText(tr("Confirm Payment"));
    cancelBtn = new BlocknetFormBtn;
    cancelBtn->setObjectName("cancel");
    cancelBtn->setText(tr("Cancel"));

    auto *btnBox = new QFrame;
    auto *btnBoxLayout = new QHBoxLayout;
    btnBoxLayout->setContentsMargins(QMargins());
    btnBoxLayout->setSpacing(15);
    btnBox->setLayout(btnBoxLayout);

    content = new QFrame;
//    content->setStyleSheet("border: 1px solid red");
    content->setObjectName("review");
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    contentLayout = new QVBoxLayout;
    contentLayout->setContentsMargins(QMargins());
    content->setLayout(contentLayout);

    layout->addWidget(titleLbl, 0, Qt::AlignTop | Qt::AlignLeft);
    layout->addSpacing(45);
    layout->addWidget(subtitleLbl, 0, Qt::AlignTop);
    layout->addSpacing(30);
    layout->addWidget(content, 1);
    layout->addSpacing(30);
    btnBoxLayout->addWidget(cancelBtn, 0, Qt::AlignLeft);
    btnBoxLayout->addWidget(continueBtn, 0, Qt::AlignLeft);
    btnBoxLayout->addStretch(1);
    layout->addWidget(btnBox);

    connect(cancelBtn, SIGNAL(clicked()), this, SLOT(onCancel()));
    connect(continueBtn, SIGNAL(clicked()), this, SLOT(onSubmit()));
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
        model->processFunds(walletModel, &coinControl);
        fillWalletData();
    }
}

/**
 * @brief Asks the wallet to create a transaction in order to obtain more accurate fee information.
 */
void BlocknetSendFunds4::fillWalletData() {
    if (model->txStatus.status != WalletModel::OK && model->txStatus.status != WalletModel::Cancel) {
        // Handle errors
        feeValueLbl->setText(tr("n/a"));
        totalValueLbl->setText(tr("n/a"));
        auto status = model->txStatus;
        warningLbl->setText(walletModel->messageForSendCoinsReturn(status));
        return;
    }

    feeValueLbl->setText(BitcoinUnits::formatWithUnit(displayUnit, model->txActiveFee()) + QString(" %1").arg(model->isZeroFee() ? tr("(attempting zero fee)") : ""));
    totalValueLbl->setText(BitcoinUnits::formatWithUnit(displayUnit, model->txTotalAmount()));
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
    auto list = model->recipients.toList();
    bool allValid = [](QList<BlocknetTransaction> &recipients, WalletModel *w, int unit) -> bool {
        for (BlocknetTransaction &rec : recipients) {
            if (!rec.isValid(w, unit))
                return false;
        }
        return true;
    }(list, walletModel, displayUnit);
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
    scrollc->setContentsMargins(0, 0, 35, 0);
    scrollc->setObjectName("content");
    auto *scrollcLayout = new QGridLayout;
    scrollcLayout->setContentsMargins(QMargins());
    scrollcLayout->setSpacing(50);
    scrollcLayout->setVerticalSpacing(0);
    scrollc->setLayout(scrollcLayout);

    // Support scrollable content
    scrollArea = new QScrollArea;
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
    QList<BlocknetTransaction> recs = model->recipients.toList();
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
        auto *boxLayout = new QHBoxLayout;
        boxLayout->setContentsMargins(QMargins());
        box->setLayout(boxLayout);
        auto *addrLbl = new QLabel(recipient.address);
        addrLbl->setObjectName("standard");
        auto *circle = new BlocknetCircle(35, 35);
        boxLayout->addWidget(circle, 0, Qt::AlignVCenter);
        boxLayout->addWidget(addrLbl, 1);
        scrollcLayout->addWidget(box, row, 0, Qt::AlignLeft | Qt::AlignVCenter);

        auto *amountLbl = new QLabel();
        amountLbl->setObjectName("standard");
        amountLbl->setText(recipient.getAmount(displayUnit));
        scrollcLayout->addWidget(amountLbl, row, 1, Qt::AlignLeft | Qt::AlignVCenter);

        auto *edit = new QPushButton(tr("Edit"));
        edit->setObjectName("linkBtn");
        edit->setCursor(Qt::PointingHandCursor);
        connect(edit, SIGNAL(clicked()), this, SLOT(onEdit()));
        scrollcLayout->addWidget(edit, row, 2, Qt::AlignRight | Qt::AlignVCenter);

        scrollcLayout->setRowMinimumHeight(row, 40);
    }
    auto *spacer = new QFrame;
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    scrollcLayout->addWidget(spacer, i + 1, 0, 1, 3);

    // Display totals

    struct Row {
        bool div; QString id; QString title; QString val; bool bold;
        explicit Row() : id(QString("div")), div(false), title(QString()), val(QString()), bold(false) { }
        explicit Row(bool div = false, QString id = QString("div"), QString title = QString(), QString val = QString(), bool bold = false) :
                div(div), id(std::move(id)), title(std::move(title)), val(std::move(val)), bold(bold) { }
    };

    const QVector<Row> rows = {
            Row{ true },
            Row{ false, QString("fees"), tr("Transaction Fee"), BitcoinUnits::formatWithUnit(displayUnit, model->txActiveFee()) },
            Row{ true },
            Row{ false, QString("total"), tr("Total"), BitcoinUnits::formatWithUnit(displayUnit, model->txTotalAmount()), true },
    };

    auto *totals = new QFrame;
//    totals->setStyleSheet("border: 1px solid red");
    auto *totalsLayout = new QGridLayout;
    totalsLayout->setContentsMargins(0, 0, 35, 0);
    totalsLayout->setSpacing(50);
    totalsLayout->setVerticalSpacing(0);
    totals->setLayout(totalsLayout);

    int k = 0;
    for (k; k < rows.count(); ++k) {
        if (rows[k].div) {
            auto *div = new BlocknetHDiv;
            totalsLayout->addWidget(div, k, 0, 1, 3);
            totalsLayout->setRowMinimumHeight(0, 15);
            continue;
        }

        auto *headerLbl = new QLabel(rows[k].title);
        headerLbl->setObjectName("header");
        totalsLayout->addWidget(headerLbl, k, 0, Qt::AlignLeft | Qt::AlignVCenter);

        auto *valLbl = new QLabel(rows[k].val);
        valLbl->setObjectName(rows[k].bold ? "header" : "standard");
        totalsLayout->addWidget(valLbl, k, 1, Qt::AlignLeft | Qt::AlignVCenter);
        if (rows[k].id == QString("fees")) {
            feeValueLbl = valLbl;
        } else if (rows[k].id == QString("total")) {
            totalValueLbl = valLbl;
        }

        auto *pl = new QLabel;
        totalsLayout->setColumnStretch(2, 1);
        totalsLayout->addWidget(pl, k, 2, Qt::AlignRight);

        totalsLayout->setRowMinimumHeight(k, 45);
    }

    warningLbl = new QLabel;
    warningLbl->setObjectName("warning");

    recipientsLayout->addWidget(scrollArea, 10);
    recipientsLayout->addWidget(totals);
    recipientsLayout->addWidget(warningLbl, 1);
    contentLayout->addWidget(recipients, 1);
}

/**
 * @brief Clears the summary data, including removing the recipients list.
 */
void BlocknetSendFunds4::clearRecipients() {
    if (recipients == nullptr)
        return;
    contentLayout->removeWidget(recipients);
    recipients->deleteLater();
    scrollArea->deleteLater();
    totalValueLbl->deleteLater();
    feeValueLbl->deleteLater();
    warningLbl->deleteLater();
    recipients = nullptr;
    scrollArea = nullptr;
    totalValueLbl = nullptr;
    feeValueLbl = nullptr;
    warningLbl = nullptr;
}

void BlocknetSendFunds4::onEdit() {
    emit edit();
}

void BlocknetSendFunds4::onSubmit() {
    if (!this->validated()) {
        return;
    }

    bool zeroFee = model->customFee && model->userFee == 0;
    if (zeroFee)
        walletModel->attemptToSendZeroFee();

    auto coinControl = model->getCoinControl(walletModel);
    auto result = model->processFunds(walletModel, &coinControl);
    if (result.status != WalletModel::OK && result.status != WalletModel::Cancel) {
        if (feeValueLbl) feeValueLbl->setText(tr("n/a"));
        if (totalValueLbl) totalValueLbl->setText(tr("n/a"));
        if (warningLbl) warningLbl->setText(walletModel->messageForSendCoinsReturn(result));
        return;
    }

    if (warningLbl) warningLbl->clear();

    if (result.status == WalletModel::OK) {
        auto *sendFundsRequest = new BlocknetSendFundsRequest(this, walletModel, &coinControl);
        result = sendFundsRequest->send(model->txRecipients, model->txFees, model->txAmount);
        if (result.status == WalletModel::OK) {
            if (zeroFee) walletModel->attemptToSendZeroFee(false);
            emit submit();
            return;
        }
    }

    if (zeroFee) // Unset
        walletModel->attemptToSendZeroFee(false);
}

void BlocknetSendFunds4::onDisplayUnit(int unit) {
    displayUnit = unit;
    displayMultiple();
    fillWalletData();
}
