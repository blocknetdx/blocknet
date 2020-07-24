// Copyright (c) 2018-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetsendfunds2.h>

#include <qt/blocknetavatar.h>
#include <qt/blocknetcheckbox.h>
#include <qt/blocknetclosebtn.h>
#include <qt/blocknetguiutil.h>
#include <qt/blocknethdiv.h>

#include <qt/addresstablemodel.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>

#include <memory>
#include <validation.h>

#include <QKeyEvent>
#include <QMessageBox>
#include <QScrollBar>
#include <QSettings>

BlocknetSendFunds2List::BlocknetSendFunds2List(int displayUnit, QFrame *parent) : QFrame(parent),
                                                                                  displayUnit(displayUnit),
                                                                                  gridLayout(new QGridLayout) {
//    this->setStyleSheet("border: 1px solid red");
    this->setObjectName(getName());
    this->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    gridLayout->setContentsMargins(0, BGU::spi(5), 0, BGU::spi(5));
    gridLayout->setSpacing(BGU::spi(12));
    this->setLayout(gridLayout);
}

QSize BlocknetSendFunds2List::sizeHint() const {
    QSize r;
    r.setWidth(this->width() + (gridLayout->contentsMargins().left() + gridLayout->contentsMargins().right()) * 4);
    int totalH = (widgets.count() / columns) * (rowHeight + gridLayout->verticalSpacing()) +
            gridLayout->contentsMargins().bottom() + gridLayout->contentsMargins().top();
    r.setHeight(totalH > 0 ? totalH : rowHeight);
    return r;
}

void BlocknetSendFunds2List::addRow(int row, const QString addr, const QString amount) {
    // col 1: Circle
    auto *circle = new BlocknetAvatar(addr, BGU::spi(35), BGU::spi(35));

    // col 2: address
    auto *addressLbl = new QLabel(addr);
    addressLbl->setObjectName("address");

    // col 3: amount
    auto *amountTi = new BlocknetLineEdit(BGU::spi(100));
    amountTi->setObjectName("amount");
    amountTi->setID(addr);
    amountTi->setPlaceholderText(tr("Enter Amount..."));
    amountTi->setValidator(new BlocknetNumberValidator(0, BLOCKNETGUI_FUNDS_MAX, BitcoinUnits::decimals(displayUnit)));
    amountTi->setMaxLength(BLOCKNETGUI_MAXCHARS);
    amountTi->setText(amount);

    // col 4: coin
    auto *coinLbl = new QLabel(BitcoinUnits::longName(displayUnit));
    coinLbl->setObjectName("coin");
    coinLbl->setFixedHeight(amountTi->minimumHeight());

    // col 5: close btn
    auto *closeBtn = new BlocknetCloseBtn;
    closeBtn->setID(addr);

    gridLayout->addWidget(circle, row, 0, Qt::AlignCenter | Qt::AlignVCenter);    gridLayout->setColumnStretch(0, 0);
    gridLayout->addWidget(addressLbl, row, 1, Qt::AlignLeft  | Qt::AlignVCenter); gridLayout->setColumnStretch(1, 0);
    gridLayout->addWidget(amountTi, row, 2, Qt::AlignLeft | Qt::AlignVCenter);    gridLayout->setColumnStretch(2, 0);
    gridLayout->addWidget(coinLbl, row, 3, Qt::AlignLeft | Qt::AlignVCenter);     gridLayout->setColumnStretch(3, 0);
    gridLayout->addWidget(closeBtn, row, 4, Qt::AlignCenter | Qt::AlignVCenter);  gridLayout->setColumnStretch(4, 0);
    gridLayout->addWidget(new QFrame, row, 5);                                    gridLayout->setColumnStretch(5, 2);
    gridLayout->setRowMinimumHeight(row, BlocknetSendFunds2List::rowHeight);      gridLayout->setRowStretch(row, 1);
    gridLayout->setColumnMinimumWidth(4, closeBtn->width() + BGU::spi(10));

    widgets << circle; widgets << addressLbl; widgets << amountTi; widgets << coinLbl; widgets << closeBtn;
    tis << amountTi;

    connect(amountTi, &BlocknetLineEdit::textEdited, this, [this](const QString & text) {
        auto *amountTi = qobject_cast<BlocknetLineEdit *>(sender());
        if (amountTi)
            Q_EMIT this->amount(amountTi->getID(), amountTi->text());
    });
    connect(closeBtn, &BlocknetCloseBtn::clicked, this, &BlocknetSendFunds2List::onRemove);
}

void BlocknetSendFunds2List::clear() {
    QLayoutItem *child;
    while ((child = gridLayout->takeAt(0)) != nullptr) {
        delete child;
    }
    QList<QWidget*> list = widgets.toList();
    for (int i = list.count() - 1; i >= 0; --i) {
        QWidget *w = list[i];
        widgets.remove(w);
        w->deleteLater();
    }
    tis.clear();
}

void BlocknetSendFunds2List::requestFocus() {
    if (!tis.empty())
        tis.first()->setFocus();
}

void BlocknetSendFunds2List::setAmount(const CAmount &amt) {
    if (tis.count() == 1) {
        tis[0]->setText(BitcoinUnits::format(displayUnit, amt));
        Q_EMIT amount(tis[0]->getID(), tis[0]->text());
    }
}

void BlocknetSendFunds2List::onRemove() {
    auto *closeBtn = qobject_cast<BlocknetCloseBtn *>(sender());
    if (closeBtn != nullptr)
        Q_EMIT remove(closeBtn->getID());
}

BlocknetSendFunds2::BlocknetSendFunds2(WalletModel *w, int id, QFrame *parent) : BlocknetSendFundsPage(w, id, parent),
                                                                                 layout(new QVBoxLayout),
                                                                                 bFundList(nullptr) {
//    this->setStyleSheet("border: 1px solid red");
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(layout);
    layout->setContentsMargins(BGU::spi(15), BGU::spi(10), BGU::spi(10), BGU::spi(30));

    titleLbl = new QLabel(tr("Send Funds"));
    titleLbl->setObjectName("h4");

    content = new QFrame;
    content->setObjectName("content");
    content->setContentsMargins(0, 0, BGU::spi(15), 0);
    content->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum);
    contentLayout = new QVBoxLayout;
    content->setLayout(contentLayout);

    scrollArea = new QScrollArea;
    scrollArea->setObjectName("contentScrollArea");
    scrollArea->setContentsMargins(QMargins());
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setWidget(content);

    auto *subtitleLbl = new QLabel(tr("How much would you like to send?"));
    subtitleLbl->setObjectName("h2");

    fundList = new QFrame;
    fundList->setObjectName("sendFundsList");
    fundList->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    auto *fundListLayout = new QVBoxLayout;
    fundListLayout->setContentsMargins(QMargins());
    fundListLayout->setSpacing(0);
    fundList->setLayout(fundListLayout);

    auto *changeBox = new QFrame;
    changeBox->setObjectName("changeSection");
    changeBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *changeBoxLayout = new QVBoxLayout;
    changeBoxLayout->setSizeConstraint(QLayout::SetMaximumSize);
    changeBoxLayout->setContentsMargins(QMargins());
    changeBoxLayout->setSpacing(0);
    changeBox->setLayout(changeBoxLayout);
    auto *changeAddrLbl = new QLabel(tr("Custom Change Address (optional)"));
    changeAddrLbl->setObjectName("sectionTitle");
    auto *changeAddrSubtitleLbl = new QLabel(tr("This will optionally put change back into a specific address of your choosing."));
    changeAddrSubtitleLbl->setObjectName("sectionSubtitle");
    changeAddrSubtitleLbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    changeAddrSubtitleLbl->setWordWrap(true);
    changeAddrTi = new BlocknetLineEdit;
    changeAddrTi->setObjectName("changeAddress");
    changeAddrTi->setFixedWidth(BGU::spi(350));
    changeAddrTi->setPlaceholderText(tr("Enter change address..."));
    changeBoxLayout->addWidget(changeAddrLbl);
    changeBoxLayout->addSpacing(BGU::spi(2));
    changeBoxLayout->addWidget(changeAddrSubtitleLbl);
    changeBoxLayout->addSpacing(BGU::spi(15));
    changeBoxLayout->addWidget(changeAddrTi);

    // Coin control options
    auto *ccBox = new QFrame;
    ccBox->setObjectName("coinControlSection");
    ccBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *ccBoxLayout = new QVBoxLayout;
    ccBoxLayout->setContentsMargins(QMargins());
    ccBoxLayout->setSpacing(BGU::spi(15));
    ccBox->setLayout(ccBoxLayout);

    auto *ccTitleLbl = new QLabel(tr("Coin Control (advanced)"));
    ccTitleLbl->setObjectName("sectionTitle");

    auto *ccSubtitleLbl = new QLabel(tr("Coin control allows you to select individual unspent transactions in your account to use when sending funds."));
    ccSubtitleLbl->setObjectName("sectionSubtitle");
    ccSubtitleLbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ccSubtitleLbl->setWordWrap(true);

    auto *ccTitleBox = new QFrame;
    ccTitleBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ccTitleBox->setLayout(new QVBoxLayout);
    ccTitleBox->layout()->setContentsMargins(QMargins());
    ccTitleBox->layout()->setSpacing(0);
    ccTitleBox->layout()->addWidget(ccTitleLbl);
    ccTitleBox->layout()->addWidget(ccSubtitleLbl);

    ccDefaultRb = new QRadioButton(tr("Default (Recommended)"));
    ccDefaultRb->setObjectName("default");
    ccManualRb = new QRadioButton(tr("Choose inputs manually"));
    ccManualRb->setObjectName("default");
    ccManualRb->setToolTip(tr("Coin Control"));

    // The manual box contains all the UI related to specifying coin inputs and
    // transaction fees manually. This includes custom change address, split utxo
    // as well as the utxo list.
    ccManualBox = new QFrame;
    ccManualBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *ccManualBoxLayout = new QHBoxLayout;
    ccManualBoxLayout->setContentsMargins(BGU::spi(15), 0, 0, 0);
    ccManualBoxLayout->setSpacing(BGU::spi(20));
    ccManualBox->setLayout(ccManualBoxLayout);

    auto *ccRightBox = new QFrame;
    ccRightBox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    auto *ccRightBoxLayout = new QVBoxLayout;
    ccRightBox->setLayout(ccRightBoxLayout);
    auto *ccSummaryLbl = new QLabel;
    ccSummaryLbl->setObjectName("h6");
    ccSummaryLbl->setText(tr("Selected Inputs"));
    ccSummary2Lbl = new QLabel;
    ccSummary2Lbl->setObjectName("body");
    ccSummary2Lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    ccSummary2Lbl->setText(tr("0 inputs have been selected"));

    auto *ccLeftBox = new QFrame;
    auto *ccLeftBoxLayout = new QVBoxLayout;
    ccLeftBoxLayout->setSpacing(BGU::spi(30));
    ccLeftBox->setLayout(ccLeftBoxLayout);

    auto *ccInputsBox = new QFrame;
    auto *ccInputsBoxLayout = new QHBoxLayout;
    ccInputsBoxLayout->setContentsMargins(QMargins());
    ccInputsBox->setLayout(ccInputsBoxLayout);
    auto *ccInputsBtn = new BlocknetFormBtn;
    ccInputsBtn->setText(tr("Select coin inputs"));
    ccInputsBoxLayout->addWidget(ccInputsBtn);

    ccLeftBoxLayout->addWidget(ccInputsBox);

    auto *changeSplitBox = new QFrame;
    auto *changeSplitBoxLayout = new QHBoxLayout;
    changeSplitBoxLayout->setContentsMargins(QMargins());
    changeSplitBoxLayout->setSpacing(BGU::spi(20));
    changeSplitBox->setLayout(changeSplitBoxLayout);
    ccSplitOutputCb = new BlocknetCheckBox(tr("Split Output"));
    ccSplitOutputCb->setMinimumWidth(BGU::spi(100));
    ccSplitOutputTi = new BlocknetLineEdit(BGU::spi(100));
    ccSplitOutputTi->setPlaceholderText(tr("# of outputs"));
    ccSplitOutputTi->setValidator(new QIntValidator(2, maxSplitOutputs));
    ccSplitOutputTi->setMaxLength(4);
    ccSplitOutputTi->setEnabled(false);
    changeSplitBoxLayout->addWidget(ccSplitOutputCb);
    changeSplitBoxLayout->addWidget(ccSplitOutputTi);
    changeSplitBoxLayout->addStretch(1);

    ccRightBoxLayout->addWidget(ccSummaryLbl, 0, Qt::AlignLeft | Qt::AlignTop);
    ccRightBoxLayout->addWidget(ccSummary2Lbl, 0, Qt::AlignLeft);
//    ccRightBoxLayout->addSpacing(BGU::spi(9));
//    ccRightBoxLayout->addWidget(changeSplitBox, 0, Qt::AlignLeft);
    ccRightBoxLayout->addStretch(1);

//    auto *ccListTreeBox = new QFrame;
//    auto *ccListTreeBoxLayout = new QHBoxLayout;
//    ccListTreeBoxLayout->setContentsMargins(QMargins());
//    ccListTreeBoxLayout->setSpacing(40);
//    ccListTreeBox->setLayout(ccListTreeBoxLayout);
//    auto *listModeRb = new QRadioButton(tr("List mode"));
//    auto *treeModeRb = new QRadioButton(tr("Tree mode"));
//    listModeRb->setChecked(true);
//    treeModeRb->setChecked(false);
//    ccListTreeBoxLayout->addWidget(listModeRb, 0, Qt::AlignLeft);
//    ccListTreeBoxLayout->addWidget(treeModeRb, 0, Qt::AlignLeft);
//    ccListTreeBoxLayout->addStretch(1);

    auto *ccManualDiv = new QLabel;
    ccManualDiv->setObjectName("vdiv");
    ccManualDiv->setFixedWidth(1);
    ccManualDiv->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    ccManualBoxLayout->addWidget(ccLeftBox, 0, Qt::AlignLeft);
    ccManualBoxLayout->addWidget(ccManualDiv, 0, Qt::AlignLeft);
    ccManualBoxLayout->addWidget(ccRightBox, 1, Qt::AlignLeft);

    // Top-level coin control layout
    ccBoxLayout->addWidget(ccTitleBox);
    ccBoxLayout->addWidget(ccDefaultRb);
    ccBoxLayout->addWidget(ccManualRb);
    ccBoxLayout->addWidget(ccManualBox);

    // Cancel/continue buttons
    auto *btnBox = new QFrame;
    btnBox->setObjectName("buttonSection");
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

    auto *hdiv1 = new BlocknetHDiv;
    auto *hdiv2 = new BlocknetHDiv;
    auto *hdiv3 = new BlocknetHDiv;

    contentLayout->addWidget(fundList, 0);
    contentLayout->addSpacing(BGU::spi(30));
    contentLayout->addWidget(hdiv1);
    contentLayout->addWidget(changeBox);
    contentLayout->addWidget(hdiv2);
    contentLayout->addWidget(ccBox);
    contentLayout->addWidget(hdiv3);
    contentLayout->addStretch(1);

    layout->addWidget(titleLbl, 0, Qt::AlignTop | Qt::AlignLeft);
    layout->addSpacing(BGU::spi(30));
    layout->addWidget(subtitleLbl);
    layout->addSpacing(BGU::spi(15));
    layout->addWidget(scrollArea, 10);
    layout->addSpacing(BGU::spi(20));
    layout->addWidget(btnBox);

    // Coin control
    ccDialog = new BlocknetCoinControlDialog(w, nullptr, Qt::WindowSystemMenuHint | Qt::WindowTitleHint);
    ccDialog->setStyleSheet(GUIUtil::loadStyleSheet());

    connect(ccDefaultRb, &QRadioButton::toggled, this, &BlocknetSendFunds2::onCoinControl);
    connect(continueBtn, &BlocknetFormBtn::clicked, this, &BlocknetSendFunds2::onNext);
    connect(cancelBtn, &BlocknetFormBtn::clicked, this, &BlocknetSendFunds2::onCancel);
    connect(backBtn, &BlocknetFormBtn::clicked, this, &BlocknetSendFunds2::onBack);
    connect(changeAddrTi, &BlocknetLineEdit::editingFinished, this, &BlocknetSendFunds2::onChangeAddress);
//    connect(ccSplitOutputCb, &QCheckBox::toggled, this, &BlocknetSendFunds2::onSplitChanged);
//    connect(ccSplitOutputTi, &BlocknetLineEdit::editingFinished, this, &BlocknetSendFunds2::onSplitChanged);
    connect(ccInputsBtn, &QPushButton::clicked, this, [this]() {
        updateCoinControl(); ccDialog->setPayAmount(model->totalRecipientsAmount()); ccDialog->show();
    });
}

void BlocknetSendFunds2::setData(BlocknetSendFundsModel *model) {
    BlocknetSendFundsPage::setData(model);
    displayUnit = walletModel->getOptionsModel()->getDisplayUnit();

    clear();

    changeAddrTi->blockSignals(true);
    changeAddrTi->setText(model->changeAddress());
    changeAddrTi->blockSignals(false);

    QSettings settings;

    // Coin control designation
    ccDefaultRb->blockSignals(true);
    ccManualRb->blockSignals(true);
    ccSplitOutputCb->blockSignals(true);
    bool showCoinControl = model->txSelectedUtxos().count() > 0 || model->split() || settings.value("nCoinControlMode").toBool();
    ccDefaultRb->setChecked(!showCoinControl);
    ccManualRb->setChecked(showCoinControl);
    ccManualBox->setHidden(!showCoinControl); // unhide the coin control box
    if (model->splitCount() > 0) // assign textinput text before split checkbox to prevent overwrite
        ccSplitOutputTi->setText(QString::number(model->splitCount()));
    ccSplitOutputCb->setChecked(model->split());
    ccDefaultRb->blockSignals(false);
    ccManualRb->blockSignals(false);
    ccSplitOutputCb->blockSignals(false);

    // summary label
    updateCoinControlSummary();

    bFundList = new BlocknetSendFunds2List(displayUnit);
    uint i = 0;
    for (const auto & r : model->txRecipients()) {
        bFundList->addRow(i, r.address, r.amount > 0 ? BitcoinUnits::format(displayUnit, r.amount) : QString());
        ++i;
    }
    fundList->layout()->addWidget(bFundList);
    fundList->adjustSize();

    connect(bFundList, &BlocknetSendFunds2List::amount, this, &BlocknetSendFunds2::onAmount);
    connect(bFundList, &BlocknetSendFunds2List::remove, this, &BlocknetSendFunds2::onRemove);
    disconnect(walletModel->getOptionsModel(), &OptionsModel::displayUnitChanged, this, &BlocknetSendFunds2::updateDisplayUnit);
    connect(walletModel->getOptionsModel(), &OptionsModel::displayUnitChanged, this, &BlocknetSendFunds2::updateDisplayUnit);
}

/**
 * Update the display unit.
 */
void BlocknetSendFunds2::updateDisplayUnit() {
    setData(model);
}

/**
 * @brief Clears the existing visual elements from the display.
 */
void BlocknetSendFunds2::clear() {
    // clear send funds list
    if (bFundList != nullptr) {
        bFundList->clear();
        bFundList->deleteLater();
    }
    bFundList = nullptr;
    for (int i = fundList->layout()->count() - 1; i >= 0; --i) {
        auto *item = fundList->layout()->itemAt(i);
        if (item->widget()->objectName() == BlocknetSendFunds2List::getName()) {
            auto *w = (BlocknetSendFunds2List*)item->widget();
            w->clear();
        }
        fundList->layout()->removeItem(item);
        item->widget()->deleteLater();
        delete item;
    }

    // reset scroll to top
    scrollArea->verticalScrollBar()->setValue(0);

    // Clear coin control lbl
    ccSummary2Lbl->setText(tr("0 inputs have been selected"));

    // Default selection
    ccDefaultRb->blockSignals(true);
    ccManualBox->blockSignals(true);
    ccDefaultRb->setChecked(true);
    ccManualBox->setHidden(true);
    ccDefaultRb->blockSignals(false);
    ccManualBox->blockSignals(false);

    // clear split
    ccSplitOutputCb->blockSignals(true);
    ccSplitOutputTi->blockSignals(true);
    ccSplitOutputCb->setChecked(false);
    ccSplitOutputTi->clear();
    ccSplitOutputTi->setEnabled(false);
    ccSplitOutputCb->blockSignals(false);
    ccSplitOutputTi->blockSignals(false);

    // clear utxo list
    ccDialog->clear();

    // clear change address
    changeAddrTi->blockSignals(true);
    changeAddrTi->clear();
    changeAddrTi->blockSignals(false);
}

/**
 * @brief  Validation check returns true if the minimum required data has been properly entered on the screen.
 * @return
 */
bool BlocknetSendFunds2::validated() {
    // Check recipients
    if (model->txRecipients().isEmpty()) {
        auto msg = tr("Please add recipients.");
        QMessageBox::warning(this->parentWidget(), tr("Issue"), msg);
        return false;
    }

    bool allTxsValid = [](const QList<SendCoinsRecipient> & recipients, WalletModel *w) -> bool {
        for (auto & r : recipients) {
            if (!w->validateAddress(r.address) || r.amount <= 0)
                return false;
        }
        return true;
    }(model->txRecipients(), walletModel);

    if (model->txRecipients().isEmpty() || !allTxsValid) {
        QString dust = BitcoinUnits::formatWithUnit(displayUnit, ::minRelayTxFee.GetFeePerK());
        auto msg = !model->txRecipients().empty() ? tr("Please specify send amounts larger than %1 for each address.").arg(dust) :
            tr("Please specify an amount larger than %1").arg(dust);
        QMessageBox::warning(this->parentWidget(), tr("Issue"), msg);
        return false;
    }

    // Check change address
    if (!changeAddrTi->text().isEmpty() && !walletModel->validateAddress(changeAddrTi->text())) {
        QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("The change address is not valid address."));
        return false;
    }

    // Check coin control split utxos
    if (ccSplitOutputCb->isChecked()) {
        bool ok = false; uint count = splitCount(&ok);
        if (!ok || count == 0) {
            QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Please enter a valid number for the split count."));
            return false;
        }
        if (this->model->txSelectedUtxos().count() > 1) { // Split requires a single utxo
            QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Please select only one UTXO with split outputs option."));
            return false;
        }
        if (this->model->txSelectedUtxos().count() < 1) {
            QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Please select a UTXO."));
            return false;
        }
        if (count > maxSplitOutputs) {
            QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Too many splits. The maximum number of splits is %n").arg(maxSplitOutputs));
            return false;
        }
    }

    // Check that coin control inputs are larger than designated amounts
    if (ccManualRb->isChecked()) {
        if (model->selectedInputsTotal() < model->totalRecipientsAmount() || model->txSelectedUtxos().empty()) {
            QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Not enough coin inputs selected to cover this transaction. An additional %1 required.")
                    .arg(BitcoinUnits::formatWithUnit(displayUnit, model->totalRecipientsAmount() - model->selectedInputsTotal())));
            return false;
        }
    }

    return true;
}

void BlocknetSendFunds2::focusInEvent(QFocusEvent *event) {
    QWidget::focusInEvent(event);
    if (bFundList != nullptr)
        bFundList->requestFocus();
}

void BlocknetSendFunds2::keyPressEvent(QKeyEvent *event) {
    QWidget::keyPressEvent(event);
    if (this->isHidden())
        return;
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
        onNext();
    else if (event->key() == Qt::Key_Escape)
        onBack();
}

void BlocknetSendFunds2::hideEvent(QHideEvent *qHideEvent) {
    if (ccDialog->isVisible())
        ccDialog->reject();
    QWidget::hideEvent(qHideEvent);
}

/**
 * @brief Sets the visible state of the manual coin control options. Refresh coin control utxos.
 */
void BlocknetSendFunds2::onCoinControl() {
    ccManualBox->setHidden(ccDefaultRb->isChecked());

    if (model) {
        if (ccDefaultRb->isChecked()) {
            model->clearTxSelectedUtxos();
            ccSummary2Lbl->clear();
            ccSplitOutputCb->setChecked(false); // triggers onSplitChanged
        }
        updateCoinControlSummary();
    }

    QSettings settings;
    settings.setValue("nCoinControlMode", ccManualRb->isChecked());
}

/**
 * @brief Assigns the change address to the fund model if it's valid.
 */
void BlocknetSendFunds2::onChangeAddress() {
    if (!walletModel || !model)
        return;

    if (!changeAddrTi->text().isEmpty() && walletModel->validateAddress(changeAddrTi->text()))
        this->model->setChangeAddress(changeAddrTi->text());
}

/**
 * @brief Called when the split count changes.
 */
void BlocknetSendFunds2::onSplitChanged() {
    this->model->setSplit(ccSplitOutputCb->isChecked());
    bool ok = false; uint count = splitCount(&ok);
    this->model->setSplitCount(this->model->split() && ok ? count : 0);
    ccSplitOutputTi->setEnabled(ccSplitOutputCb->isChecked());

    if (ccSplitOutputCb->isChecked()) {
        auto pts = this->model->selectedOutpoints();
        auto outputs = walletModel->wallet().getCoins(pts);

        CAmount totalN{0};
        for (auto & utxo : outputs)
            totalN += utxo.txout.nValue;

        bFundList->setAmount(totalN);
    } else {
        ccSplitOutputTi->clear();
    }
}

/**
 * @brief Handles the amount signal from the send funds list. This method will convert the user specified amount
 *        into a double with a precision of 8. The max supported precision for Blocknet is 1/100000000. This
 *        method will mutate the transaction list with the new amount.
 * @param addr Address of the transaction
 * @param amount Amount of the transaction
 */
void BlocknetSendFunds2::onAmount(QString addr, QString amount) {
    if (model->txRecipients().empty()) // should not be empty here
        return;

    CAmount newAmount = BlocknetSendFundsModel::stringToInt(amount, displayUnit);

    // remove existing recipient
    if (model->hasRecipient(addr))
        model->removeRecipient(addr);

    SendCoinsRecipient recipient;
    recipient.address = addr;
    recipient.amount = newAmount;
    recipient.label = walletModel->getAddressTableModel()->labelForAddress(addr);
    model->addRecipient(recipient);
}

/**
 * @brief Handles the remove signal from the address list.
 * @param addr Address of the sender to remove
 */
void BlocknetSendFunds2::onRemove(const QString addr) {
    model->removeRecipient(addr);
    if (model->txRecipients().empty()) // go back to address screen if no more recipients
        Q_EMIT back(this->pageID);
    else setData(this->model);
}

/**
 * @brief Responsible for building the coin control data provider. This method will query the unspent transactions
 *        (UTXOs) in the wallet and store in a data model for use with the coin control dialog.
 */
void BlocknetSendFunds2::updateCoinControl() {
    disconnect(ccDialog, &BlocknetCoinControlDialog::accepted, this, &BlocknetSendFunds2::ccAccepted);
    ccDialog->populateUnspentTransactions(model->txSelectedUtxos());
    connect(ccDialog, &BlocknetCoinControlDialog::accepted, this, &BlocknetSendFunds2::ccAccepted);
}

void BlocknetSendFunds2::ccAccepted() {
    // Get selected utxos
    QVector<BlocknetSimpleUTXO> selectedUtxos;
    for (auto *data : ccDialog->getCC()->getData()->data) {
        if (data->checked) {
            BlocknetSimpleUTXO utxo(uint256S(data->transaction.toStdString()), data->vout, data->address, data->camount);
            selectedUtxos.push_back(utxo);
        }
        if (data->locked) {
            COutPoint utxo(uint256S(data->transaction.toStdString()), data->vout);
            walletModel->wallet().lockCoin(utxo);
        }
        if (data->unlocked) {
            COutPoint utxo(uint256S(data->transaction.toStdString()), data->vout);
            walletModel->wallet().unlockCoin(utxo);
        }
    }
    this->model->setTxSelectedUtxos(selectedUtxos);
    updateCoinControlSummary();
}

uint BlocknetSendFunds2::splitCount(bool *ok) {
    uint count = ccSplitOutputTi->text().toUInt(ok);
    count = (ok ? count : 0);
    return count;
}

void BlocknetSendFunds2::updateCoinControlSummary() {
    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    int utxoCount = this->model->txSelectedUtxos().count();
    CAmount totalN = 0;
    QSet<QString> addrs;
    for (auto &t : this->model->txSelectedUtxos()) {
        totalN += t.amount;
        addrs.insert(t.address);
    }

    // update the selected inputs summary
    QString c = QString::number(utxoCount);
    QString a = QString::number(addrs.count());
    QString s1 = tr("%1 input has been selected from %2 addresses totalling %3").arg(c, a, BitcoinUnits::formatWithUnit(unit, totalN));
    QString s2 = tr("%1 inputs have been selected from %2 addresses totalling %3").arg(c, a, BitcoinUnits::formatWithUnit(unit, totalN));
    QString sNone = tr("0 inputs have been selected");

    if (utxoCount == 0)
        ccSummary2Lbl->setText(sNone);
    else
        ccSummary2Lbl->setText(utxoCount == 1 ? s1 : s2);
}
