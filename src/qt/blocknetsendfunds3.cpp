// Copyright (c) 2018-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetsendfunds3.h>

#include <qt/blocknetcheckbox.h>
#include <qt/blocknethdiv.h>
#include <qt/blocknetguiutil.h>

#include <qt/bitcoinunits.h>
#include <qt/optionsmodel.h>

#include <wallet/coincontrol.h>
#include <validation.h>

#include <QKeyEvent>
#include <QMessageBox>

BlocknetSendFunds3::BlocknetSendFunds3(WalletModel *w, int id, QFrame *parent) : BlocknetSendFundsPage(w, id, parent),
                                                                                 layout(new QVBoxLayout)
{
//    this->setStyleSheet("border: 1px solid red");
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(layout);
    layout->setContentsMargins(BGU::spi(15), BGU::spi(10), BGU::spi(35), BGU::spi(30));

    titleLbl = new QLabel(tr("Send Funds"));
    titleLbl->setObjectName("h4");

    auto *subtitleLbl = new QLabel(tr("Choose transaction fee"));
    subtitleLbl->setObjectName("h2");

    auto *rbBox = new QFrame;
    rbBox->setObjectName("radioBox");
    rbBox->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    auto *rbBoxLayout = new QVBoxLayout;
    rbBoxLayout->setContentsMargins(QMargins());
    rbBoxLayout->setSpacing(BGU::spi(20));
    rbBox->setLayout(rbBoxLayout);
    recommendedRb = new QRadioButton(tr("Recommended fee"));
    recommendedRb->setObjectName("defaultRb");
    specificRb = new QRadioButton(tr("Specific total fee"));
    specificRb->setObjectName("defaultRb");
    recommendedDescLbl = new QLabel;
    recommendedDescLbl->setObjectName("descLbl");
    auto *specificBox = new QFrame;
    specificBox->setObjectName("specificFeeBox");
    specificBox->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    auto *specificBoxLayout = new QHBoxLayout;
    specificBoxLayout->setContentsMargins(QMargins());
    specificBoxLayout->setSpacing(BGU::spi(10));
    specificBox->setLayout(specificBoxLayout);
    specificFeeTi = new BlocknetLineEdit;
    specificFeeTi->setPlaceholderText(tr("Enter Amount..."));
    specificFeeTi->setMaxLength(18);
    specificFeeTi->setFocusPolicy(Qt::FocusPolicy::ClickFocus);
    specificFeeLbl = new QLabel;
    specificFeeLbl->setObjectName("coin");
    specificFeeLbl->setFixedHeight(specificFeeTi->minimumHeight());
    specificBoxLayout->addWidget(specificFeeTi, 0, Qt::AlignLeft);
    specificBoxLayout->addWidget(specificFeeLbl, 0, Qt::AlignLeft);
    specificBoxLayout->addStretch(1);

    rbBoxLayout->addWidget(recommendedRb);
    rbBoxLayout->addWidget(recommendedDescLbl);
    rbBoxLayout->addSpacing(BGU::spi(10));
    rbBoxLayout->addWidget(specificRb);
    rbBoxLayout->addWidget(specificBox);

    auto *hdiv1 = new BlocknetHDiv;
    auto *hdiv2 = new BlocknetHDiv;
    auto *hdiv3 = new BlocknetHDiv;

    auto *totalBox = new QFrame;
    totalBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    auto *totalBoxLayout = new QHBoxLayout;
    totalBoxLayout->setContentsMargins(QMargins());
    totalBoxLayout->setSpacing(BGU::spi(60));
    totalBox->setLayout(totalBoxLayout);
    auto *totalLbl = new QLabel(tr("Transaction Fee"));
    totalLbl->setObjectName("totalLbl");
    totalFeeLbl = new QLabel;
    totalFeeLbl->setObjectName("totalFeeLbl");
    totalBoxLayout->addWidget(totalLbl, 0, Qt::AlignLeft);
    totalBoxLayout->addWidget(totalFeeLbl, 0, Qt::AlignLeft);
    totalBoxLayout->addStretch(1);

    subtractFeeCb = new BlocknetCheckBox(tr("Subtract fee from total"));
    subtractFeeCb->setObjectName("subtractFeeCb");
    subtractFeeCb->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    transactionFeeDesc = new QLabel(tr("A zero fee transaction will be attempted, but may fail"));
    transactionFeeDesc->setObjectName("descLbl");
    transactionFeeDesc->hide();

    warningLbl = new QLabel;
    warningLbl->setObjectName("warning");

    auto *btnBox = new QFrame;
    btnBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
    auto *btnBoxLayout = new QHBoxLayout;
    btnBoxLayout->setContentsMargins(QMargins());
    btnBoxLayout->setSpacing(BGU::spi(15));
    btnBox->setLayout(btnBoxLayout);
    auto *backBtn = new BlocknetFormBtn;
    backBtn->setText(tr("Back"));
    continueBtn = new BlocknetFormBtn;
    continueBtn->setText(tr("Continue"));
    cancelBtn = new BlocknetFormBtn;
    cancelBtn->setObjectName("cancel");
    cancelBtn->setText(tr("Cancel"));
    btnBoxLayout->addWidget(backBtn, 0, Qt::AlignLeft | Qt::AlignBottom);
    btnBoxLayout->addWidget(continueBtn, 0, Qt::AlignLeft | Qt::AlignBottom);
    btnBoxLayout->addStretch(1);
    btnBoxLayout->addWidget(cancelBtn, 0, Qt::AlignRight | Qt::AlignBottom);

    layout->addWidget(titleLbl, 0, Qt::AlignTop | Qt::AlignLeft);
    layout->addSpacing(BGU::spi(30));
    layout->addWidget(subtitleLbl, 0, Qt::AlignTop);
    layout->addSpacing(BGU::spi(20));
    layout->addWidget(rbBox);
    layout->addSpacing(BGU::spi(25));
    layout->addWidget(hdiv1);
    layout->addSpacing(BGU::spi(20));
    layout->addWidget(subtractFeeCb);
    layout->addSpacing(BGU::spi(20));
    layout->addWidget(hdiv2);
    layout->addSpacing(BGU::spi(20));
    layout->addWidget(totalBox);
    layout->addWidget(transactionFeeDesc);
    layout->addSpacing(BGU::spi(20));
    layout->addWidget(hdiv3);
    layout->addSpacing(BGU::spi(10));
    layout->addWidget(warningLbl);
    layout->addSpacing(BGU::spi(20));
    layout->addStretch(1);
    layout->addWidget(btnBox);

    recommendedRb->setChecked(true);

    connect(recommendedRb, &QRadioButton::toggled, this, &BlocknetSendFunds3::onFeeDesignation);
    connect(subtractFeeCb, &QCheckBox::toggled, this, &BlocknetSendFunds3::onSubtractFee);
    connect(specificFeeTi, &BlocknetLineEdit::editingFinished, this, &BlocknetSendFunds3::onSpecificFee);
    connect(specificFeeTi, &QLineEdit::textEdited, [this](const QString &text) {
        if (text.isEmpty() && model->userFee() != 0)
            updateFee();
    });
    connect(continueBtn, &BlocknetFormBtn::clicked, this, &BlocknetSendFunds3::onNext);
    connect(cancelBtn, &BlocknetFormBtn::clicked, this, &BlocknetSendFunds3::onCancel);
    connect(backBtn, &BlocknetFormBtn::clicked, this, &BlocknetSendFunds3::onBack);
}

/**
 * @brief Set the display unit and validator on the specific fee input. Updates the fee information.
 * @param data
 */
void BlocknetSendFunds3::setData(BlocknetSendFundsModel *model) {
    BlocknetSendFundsPage::setData(model);
    displayUnit = walletModel->getOptionsModel()->getDisplayUnit();

    recommendedRb->blockSignals(true);
    specificFeeTi->blockSignals(true);
    subtractFeeCb->blockSignals(true);
    specificRb->blockSignals(true);
    recommendedRb->setChecked(!model->customFee());
    specificRb->setChecked(model->customFee());
    subtractFeeCb->setChecked(model->subtractFee());
    specificFeeTi->setValidator(new BlocknetNumberValidator(0, BLOCKNETGUI_FUNDS_MAX, BitcoinUnits::decimals(displayUnit)));
    if (model->customFee())
        specificFeeTi->setText(BitcoinUnits::format(displayUnit, model->userFee()));
    recommendedRb->blockSignals(false);
    specificRb->blockSignals(false);
    specificFeeTi->blockSignals(false);
    subtractFeeCb->blockSignals(false);

    updateFee();
}

/**
 * @brief Clear fee information from labels.
 */
void BlocknetSendFunds3::clear() {
    recommendedRb->blockSignals(true);
    specificFeeTi->blockSignals(true);
    specificRb->blockSignals(true);
    subtractFeeCb->blockSignals(true);
    specificFeeTi->clear();
    specificFeeLbl->clear();
    totalFeeLbl->clear();
    recommendedRb->setChecked(true);
    specificRb->setChecked(false);
    subtractFeeCb->setChecked(false);
    warningLbl->clear();
    recommendedRb->blockSignals(false);
    specificRb->blockSignals(false);
    specificFeeTi->blockSignals(false);
    subtractFeeCb->blockSignals(false);
}

/**
 * @brief No validation required for this screen, since the fee defaults to 0 if no information is provided
 *        in the specific fee designation.
 */
bool BlocknetSendFunds3::validated() {
    if (specificRb->isChecked()) {
        auto fee = BlocknetSendFundsModel::stringToDouble(specificFeeTi->text().isEmpty() ? "0" : specificFeeTi->text(), displayUnit);
        if (BlocknetSendFundsModel::doubleToInt(fee, displayUnit) > ::maxTxFee) {
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
    connect(walletModel, &WalletModel::encryptionStatusChanged, this, &BlocknetSendFunds3::onEncryptionStatus);
    connect(walletModel->getOptionsModel(), &OptionsModel::displayUnitChanged, this, &BlocknetSendFunds3::onDisplayUnit);
}

void BlocknetSendFunds3::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    disconnect(walletModel, &WalletModel::encryptionStatusChanged, this, &BlocknetSendFunds3::onEncryptionStatus);
    disconnect(walletModel->getOptionsModel(), &OptionsModel::displayUnitChanged, this, &BlocknetSendFunds3::onDisplayUnit);
}

void BlocknetSendFunds3::onFeeDesignation() {
    specificFeeTi->setEnabled(specificRb->isChecked());
    if (specificRb->isChecked()) {
        if (!specificFeeTi->hasFocus())
            specificFeeTi->setFocus();
        if (specificFeeTi->text().isEmpty()) {
            specificFeeTi->blockSignals(true);
            specificFeeTi->setText(BitcoinUnits::format(displayUnit, model->txFees()));
            specificFeeTi->blockSignals(false);
        }
        onSpecificFee();
    } else
        updateFee();
}

void BlocknetSendFunds3::onSpecificFee() {
    // Only update if the custom fee flag is dirty and the user fee is different
    // than what's currently specified
    if (specificRb->isChecked() != model->customFee()
        || BlocknetSendFundsModel::stringToInt(specificFeeTi->text(), displayUnit) != model->userFee())
        updateFee();
}

void BlocknetSendFunds3::onEncryptionStatus() {
    if (!this->isHidden() && walletModel->getEncryptionStatus() == WalletModel::Unlocked)
        updateFee();
}

void BlocknetSendFunds3::onSubtractFee() {
    model->setSubtractFee(subtractFeeCb->isChecked());
    updateFee();
}

void BlocknetSendFunds3::updateFee() {
    specificFeeLbl->setText(BitcoinUnits::longName(displayUnit));

    // If specific fee option is selected, do not display per kB
    // since it's a total fee and not an estimate.
    if (specificRb->isChecked()) {
        double sfee = 0;
        if (!specificFeeTi->text().isEmpty())
            sfee = BlocknetSendFundsModel::stringToDouble(specificFeeTi->text(), displayUnit);
        CAmount cfee = BlocknetSendFundsModel::doubleToInt(sfee, displayUnit);
        updateModelTxFees(cfee);
        totalFeeLbl->setText(BitcoinUnits::formatWithUnit(displayUnit, cfee));
        model->setEstimatedFees(cfee);
    } else {
        updateModelTxFees(0);
        CCoinControl cc = model->getCoinControl(walletModel);
        if (!walletModel->wallet().isLocked()) { // if the wallet is unlocked, calculate exact fees
            auto status = model->prepareFunds(walletModel, cc);
            QString feeAmount = BitcoinUnits::formatWithUnit(displayUnit, model->txFees());
            QString feeMsg =  tr("Estimated to begin confirmation within 1 block");
            recommendedDescLbl->setText(QString("%1\n%2").arg(feeAmount, feeMsg));
            totalFeeLbl->setText(feeAmount);
            // Display or clear the warning message according to the wallet status
            if (status.status != WalletModel::OK)
                warningLbl->setText(processSendCoinsReturn(walletModel, status).first);
            else warningLbl->clear();
        } else if (cc.HasSelected()) { // estimate b/c wallet is locked here
            const auto feeInfo = BlocknetEstimateFee(walletModel, cc, model->subtractFee(), model->txRecipients());
            QString feeAmount = BitcoinUnits::formatWithUnit(displayUnit, std::get<0>(feeInfo));
            QString feeMsg =  tr("Estimated to begin confirmation within 1 block. Unlock the wallet for more accurate fees");
            recommendedDescLbl->setText(QString("%1\n%2").arg(feeAmount, feeMsg));
            totalFeeLbl->setText(QString("%1 %2").arg(feeAmount, tr("(estimated)")));
            model->setEstimatedFees(std::get<0>(feeInfo));
        } else { // estimate b/c wallet is locked here
            cc.m_feerate.reset(); // explicitly use only fee estimation rate for smart fee labels
            int estimatedConfirmations;
            FeeReason reason;
            CFeeRate feeRate = CFeeRate(walletModel->wallet().getMinimumFee(1000, cc, &estimatedConfirmations, &reason));
            QString feeAmount = BitcoinUnits::formatWithUnit(displayUnit, feeRate.GetFeePerK()) + "/kB";
            QString feeMsg =  tr("Estimated to begin confirmation within %1 block(s). Unlock the wallet for more accurate fees")
                    .arg(estimatedConfirmations);
            recommendedDescLbl->setText(QString("%1\n%2").arg(feeAmount, feeMsg));
            totalFeeLbl->setText(QString("%1 %2").arg(feeAmount, tr("(estimated)")));
            model->setEstimatedFees(feeRate.GetFeePerK());
        }
    }

    // Remove focus on update fee
    if (specificFeeTi->hasFocus()) {
        specificFeeTi->blockSignals(true);
        specificFeeTi->clearFocus();
        specificFeeTi->blockSignals(false);
    }
}

void BlocknetSendFunds3::updateModelTxFees(CAmount fee) {
    model->setUserFee(fee);
    model->setCustomFee(specificRb->isChecked());
}

void BlocknetSendFunds3::onDisplayUnit(int unit) {
    setData(model);
}