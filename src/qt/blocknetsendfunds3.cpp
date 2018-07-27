// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetsendfunds3.h"
#include "blocknethdiv.h"

#include "bitcoinunits.h"
#include "optionsmodel.h"
#include "coincontrol.h"

#include <QDoubleValidator>
#include <QKeyEvent>
#include <QMessageBox>

BlocknetSendFunds3::BlocknetSendFunds3(WalletModel *w, int id, QFrame *parent) : BlocknetSendFundsPage(w, id, parent),
                                                                 layout(new QVBoxLayout) {
//    this->setStyleSheet("border: 1px solid red");
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(layout);
    layout->setContentsMargins(15, 10, 35, 30);

    titleLbl = new QLabel(tr("Send Funds"));
    titleLbl->setObjectName("h4");

    QLabel *subtitleLbl = new QLabel(tr("Choose transaction fee"));
    subtitleLbl->setObjectName("h2");

    auto *rbBox = new QFrame;
    rbBox->setObjectName("radioBox");
    auto *rbBoxLayout = new QVBoxLayout;
    rbBoxLayout->setContentsMargins(QMargins());
    rbBoxLayout->setSpacing(20);
    rbBox->setLayout(rbBoxLayout);
    recommendedRb = new QRadioButton(tr("Recommended fee"));
    recommendedRb->setObjectName("defaultRb");
    specificRb = new QRadioButton(tr("Specific total fee"));
    specificRb->setObjectName("defaultRb");
    recommendedDescLbl = new QLabel;
    recommendedDescLbl->setObjectName("descLbl");
    auto *specificBox = new QFrame;
    specificBox->setObjectName("specificFeeBox");
    auto *specificBoxLayout = new QHBoxLayout;
    specificBoxLayout->setContentsMargins(QMargins());
    specificBoxLayout->setSpacing(10);
    specificBox->setLayout(specificBoxLayout);
    specificFeeTi = new BlocknetLineEdit;
    specificFeeTi->setPlaceholderText(tr("Enter Amount..."));
    specificFeeTi->setMaxLength(18);
    specificFeeTi->setFocusPolicy(Qt::FocusPolicy::ClickFocus);
    specificFeeLbl = new QLabel;
    specificFeeLbl->setObjectName("specificFeeLbl");
    specificFeeLbl->setFixedHeight(specificFeeTi->minimumHeight());
    specificBoxLayout->addWidget(specificFeeTi, 0, Qt::AlignLeft);
    specificBoxLayout->addWidget(specificFeeLbl, 0, Qt::AlignLeft);
    specificBoxLayout->addStretch(1);

    rbBoxLayout->addWidget(recommendedRb);
    rbBoxLayout->addWidget(recommendedDescLbl);
    rbBoxLayout->addSpacing(15);
    rbBoxLayout->addWidget(specificRb);
    rbBoxLayout->addWidget(specificBox);

    auto *hdiv1 = new BlocknetHDiv;
    auto *hdiv2 = new BlocknetHDiv;

    auto *totalBox = new QFrame;
    auto *totalBoxLayout = new QHBoxLayout;
    totalBoxLayout->setContentsMargins(QMargins());
    totalBoxLayout->setSpacing(100);
    totalBox->setLayout(totalBoxLayout);
    auto *totalLbl = new QLabel(tr("Transaction Fee"));
    totalLbl->setObjectName("totalLbl");
    totalFeeLbl = new QLabel;
    totalFeeLbl->setObjectName("totalFeeLbl");
    totalBoxLayout->addWidget(totalLbl, 0, Qt::AlignLeft);
    totalBoxLayout->addWidget(totalFeeLbl, 0, Qt::AlignLeft);
    totalBoxLayout->addStretch(1);

    transactionFeeDesc = new QLabel(tr("A zero fee transaction will be attempted, but may fail"));
    transactionFeeDesc->setObjectName("descLbl");
    transactionFeeDesc->hide();

    auto *btnBox = new QFrame;
    auto *btnBoxLayout = new QHBoxLayout;
    btnBoxLayout->setContentsMargins(QMargins());
    btnBoxLayout->setSpacing(15);
    btnBox->setLayout(btnBoxLayout);
    continueBtn = new BlocknetFormBtn;
    continueBtn->setText(tr("Continue"));
    cancelBtn = new BlocknetFormBtn;
    cancelBtn->setObjectName("cancel");
    cancelBtn->setText(tr("Cancel"));
    btnBoxLayout->addWidget(cancelBtn, 0, Qt::AlignLeft);
    btnBoxLayout->addWidget(continueBtn, 0, Qt::AlignLeft);
    btnBoxLayout->addStretch(1);

    layout->addWidget(titleLbl, 0, Qt::AlignTop | Qt::AlignLeft);
    layout->addSpacing(45);
    layout->addWidget(subtitleLbl, 0, Qt::AlignTop);
    layout->addSpacing(25);
    layout->addWidget(rbBox);
    layout->addSpacing(45);
    layout->addWidget(hdiv1);
    layout->addSpacing(30);
    layout->addWidget(totalBox);
    layout->addWidget(transactionFeeDesc);
    layout->addSpacing(30);
    layout->addWidget(hdiv2);
    layout->addSpacing(60);
    layout->addWidget(btnBox);
    layout->addStretch(1);

    recommendedRb->setChecked(true);

    connect(recommendedRb, SIGNAL(toggled(bool)), this, SLOT(onFeeDesignation()));
    connect(specificFeeTi, SIGNAL(textChanged(const QString&)), this, SLOT(onSpecificFee(const QString&)));
    connect(continueBtn, SIGNAL(clicked()), this, SLOT(onNext()));
    connect(cancelBtn, SIGNAL(clicked()), this, SLOT(onCancel()));
}

/**
 * @brief Set the display unit and validator on the specific fee input. Updates the fee information.
 * @param data
 */
void BlocknetSendFunds3::setData(BlocknetSendFundsModel *model) {
    BlocknetSendFundsPage::setData(model);
    displayUnit = walletModel->getOptionsModel()->getDisplayUnit();
    specificFeeTi->setValidator(new QDoubleValidator(0, BLOCKNETGUI_FUNDS_MAX, BitcoinUnits::decimals(displayUnit)));
    specificFeeLbl->setText(BitcoinUnits::name(displayUnit));
    auto coinControl = model->getCoinControl(walletModel);
    model->processFunds(walletModel, &coinControl);
    updateFee();
}

/**
 * @brief Clear fee information from labels.
 */
void BlocknetSendFunds3::clear() {
    recommendedRb->blockSignals(true);
    specificFeeTi->blockSignals(true);
    specificRb->blockSignals(true);
    specificFeeTi->clear();
    specificFeeLbl->clear();
    totalFeeLbl->clear();
    recommendedRb->setChecked(true);
    specificRb->setChecked(false);
    recommendedRb->blockSignals(false);
    specificRb->blockSignals(false);
    specificFeeTi->blockSignals(false);
}

/**
 * @brief No validation required for this screen, since the fee defaults to 0 if no information is provided
 *        in the specific fee designation.
 */
bool BlocknetSendFunds3::validated() {
    if (specificRb->isChecked()) {
        auto fee = BlocknetTransaction::stringToDouble(specificFeeTi->text().isEmpty() ? "0" : specificFeeTi->text(), displayUnit);
        if (walletModel->isInsaneFee(BlocknetTransaction::doubleToInt(fee, displayUnit))) {
            QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("You specified a very large fee."));
            return false;
        }
    }
    return true;
}

void BlocknetSendFunds3::keyPressEvent(QKeyEvent *event) {
    QWidget::keyPressEvent(event);
    if (this->isHidden())
        return;
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
        onNext();
    else if (event->key() == Qt::Key_Escape)
        onBack();
}

void BlocknetSendFunds3::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    connect(walletModel, SIGNAL(encryptionStatusChanged(int)), this, SLOT(onEncryptionStatus(int)));
}

void BlocknetSendFunds3::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    disconnect(walletModel, SIGNAL(encryptionStatusChanged(int)), this, SLOT(onEncryptionStatus(int)));
}

void BlocknetSendFunds3::onFeeDesignation() {
    specificFeeTi->setEnabled(specificRb->isChecked());
    if (specificRb->isChecked())
        onSpecificFee();
    else
        updateFee();
}

void BlocknetSendFunds3::onSpecificFee(const QString&) {
    if (specificRb->isChecked())
        specificFeeTi->setFocus();
    updateFee();
}

void BlocknetSendFunds3::onEncryptionStatus(int encStatus) {
    if (!this->isHidden() && encStatus == WalletModel::Unlocked) {
        auto coinControl = model->getCoinControl(walletModel);
        model->processFunds(walletModel, &coinControl);
        updateFee();
    }
}

void BlocknetSendFunds3::updateFee() {
    CFeeRate feeRate = walletModel->estimatedFee(1);
    recommendedDescLbl->setText(QString("%1\n%2").arg(BitcoinUnits::formatWithUnit(displayUnit, feeRate.GetFeePerK()) + "/kB",
                                                      tr("Estimated to begin confirmation in %1 block").arg(1)));

    // If specific fee option is selected, do not display per kB
    // since it's a total fee and not an estimate.
    if (specificRb->isChecked()) {
        double sfee = 0;
        if (!specificFeeTi->text().isEmpty())
            sfee = BlocknetTransaction::stringToDouble(specificFeeTi->text(), displayUnit);
        CAmount cfee = BlocknetTransaction::doubleToInt(sfee, displayUnit);
        showZeroFeeMsg(cfee == 0);
        updateTxFees(cfee);
        totalFeeLbl->setText(BitcoinUnits::formatWithUnit(displayUnit, cfee));
        return;
    } else {
        updateTxFees(0);
    }

    showZeroFeeMsg(false);
    totalFeeLbl->setText(BitcoinUnits::formatWithUnit(displayUnit, model->txActiveFee()));
}

void BlocknetSendFunds3::showZeroFeeMsg(bool show) {
    transactionFeeDesc->setHidden(!show);
}

void BlocknetSendFunds3::updateTxFees(CAmount fee) {
    model->userFee = fee;
    model->customFee = specificRb->isChecked();
}
