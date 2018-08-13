// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetsendfunds2.h"
#include "blocknethdiv.h"
#include "blocknetcircle.h"

#include "optionsmodel.h"

#include <QCheckBox>
#include <QDoubleValidator>
#include <QMessageBox>
#include <QEvent>
#include <QKeyEvent>

BlocknetSendFunds2List::BlocknetSendFunds2List(int displayUnit, QFrame *parent) : QFrame(parent),
                                                                                  displayUnit(displayUnit),
                                                                                  gridLayout(new QGridLayout) {
//    this->setStyleSheet("border: 1px solid red");
    this->setObjectName(getName());
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    gridLayout->setContentsMargins(0, 5, 0, 5);
    gridLayout->setSpacing(15);
    this->setLayout(gridLayout);
}

QSize BlocknetSendFunds2List::sizeHint() const {
    QSize r;
    r.setWidth(this->width());
    int totalH = (widgets.count() / columns) * (rowHeight + gridLayout->verticalSpacing()) +
            gridLayout->contentsMargins().bottom() + gridLayout->contentsMargins().top();
    r.setHeight(totalH > 0 ? totalH : rowHeight);
    return r;
}

void BlocknetSendFunds2List::addRow(int row, const QString addr, const QString amount) {
    // col 1: Circle
    auto *circle = new BlocknetCircle(50, 50);

    // col 2: address
    auto *addressLbl = new QLabel(addr);
    addressLbl->setObjectName("address");

    // col 3: amount
    auto *amountTi = new BlocknetLineEdit;
    amountTi->setID(addr);
    amountTi->setPlaceholderText(tr("Enter Amount..."));
    amountTi->setObjectName("amount");
    auto validator = new QDoubleValidator(0, BLOCKNETGUI_FUNDS_MAX, BitcoinUnits::decimals(displayUnit));
    amountTi->setValidator(validator);
    amountTi->setMaxLength(BLOCKNETGUI_MAXCHARS);
    amountTi->setText(amount);

    // col 4: coin
    auto *coinLbl = new QLabel(BitcoinUnits::name(displayUnit));
    coinLbl->setObjectName("coin");
    coinLbl->setFixedHeight(amountTi->minimumHeight());

    gridLayout->addWidget(circle, row, 0, Qt::AlignCenter | Qt::AlignVCenter);    gridLayout->setColumnStretch(0, 0);
    gridLayout->addWidget(addressLbl, row, 1, Qt::AlignLeft  | Qt::AlignVCenter); gridLayout->setColumnStretch(1, 1);
    gridLayout->addWidget(amountTi, row, 2, Qt::AlignLeft | Qt::AlignVCenter);    gridLayout->setColumnStretch(2, 0);
    gridLayout->addWidget(coinLbl, row, 3, Qt::AlignLeft | Qt::AlignVCenter);     gridLayout->setColumnStretch(3, 2);
    gridLayout->setRowMinimumHeight(row, BlocknetSendFunds2List::rowHeight);      gridLayout->setRowStretch(row, 1);

    widgets << circle; widgets << addressLbl; widgets << amountTi; widgets << coinLbl;
    tis << amountTi;

    connect(amountTi, SIGNAL(editingFinished()), this, SLOT(onAmount()));
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

void BlocknetSendFunds2List::onAmount() {
    auto *amountTi = qobject_cast<BlocknetLineEdit *>(sender());
    if (amountTi != nullptr)
        emit amount(amountTi->getID(), amountTi->text());
}

BlocknetSendFunds2::BlocknetSendFunds2(WalletModel *w, int id, QFrame *parent) : BlocknetSendFundsPage(w, id, parent),
                                                                                 layout(new QVBoxLayout),
                                                                                 bFundList(nullptr) {
//    this->setStyleSheet("border: 1px solid red");
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(layout);
    layout->setContentsMargins(15, 10, 10, 30);

    titleLbl = new QLabel(tr("Send Funds"));
    titleLbl->setObjectName("h4");

    content = new QFrame;
    content->setObjectName("content");
    contentLayout = new QVBoxLayout;
    contentLayout->setContentsMargins(0, 0, 30, 0);
    content->setLayout(contentLayout);

    scrollArea = new QScrollArea;
    scrollArea->setContentsMargins(QMargins());
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(content);

    QLabel *subtitleLbl = new QLabel(tr("How much would you like to send?"));
    subtitleLbl->setObjectName("h2");

    fundList = new QFrame;
//    fundList->setStyleSheet("border: 1px solid red");
    fundList->setObjectName("sendFundsList");
    fundList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *fundListLayout = new QVBoxLayout;
    fundListLayout->setContentsMargins(QMargins());
    fundListLayout->setSpacing(0);
    fundList->setLayout(fundListLayout);

    continueBtn = new BlocknetFormBtn;
    continueBtn->setText(tr("Continue"));
    cancelBtn = new BlocknetFormBtn;
    cancelBtn->setObjectName("cancel");
    cancelBtn->setText(tr("Cancel"));

    // Coin control options
    auto *ccBox = new QFrame;
//    ccBox->setStyleSheet("border: 1px solid red");
    ccBox->setObjectName("coinControlSection");
    ccBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *ccBoxLayout = new QVBoxLayout;
    ccBoxLayout->setContentsMargins(QMargins());
    ccBoxLayout->setSpacing(20);
    ccBox->setLayout(ccBoxLayout);

    auto *ccTitleLbl = new QLabel(tr("Coin Control Options"));
    ccTitleLbl->setObjectName("sectionTitle");

    auto *ccSubtitleLbl = new QLabel(tr("Default (Recommended)"));
    ccSubtitleLbl->setObjectName("sectionSubtitle");

    ccDefaultRb = new QRadioButton(tr("Default (Recommended)"));
    ccDefaultRb->setObjectName("default");

    ccManualRb = new QRadioButton(tr("Choose inputs manually"));
    ccManualRb->setObjectName("default");
    ccManualRb->setEnabled(false); // TODO Support manual coin control
    ccManualRb->setToolTip(tr("Coming soon"));

    // The manual box contains all the UI related to specifying coin inputs and
    // transaction fees manually. This includes custom change address, split utxo
    // as well as the utxo list.
    ccManualBox = new QFrame;
    auto *ccManualBoxLayout = new QVBoxLayout;
    ccManualBoxLayout->setContentsMargins(28, 0, 0, 0);
    ccManualBoxLayout->setSpacing(20);
    ccManualBox->setLayout(ccManualBoxLayout);

    auto *changeSplitBox = new QFrame;
    auto *changeSplitBoxLayout = new QHBoxLayout;
    changeSplitBoxLayout->setContentsMargins(QMargins());
    changeSplitBox->setLayout(changeSplitBoxLayout);
    auto *ccSplitUTXOCb = new QCheckBox(tr("Split UTXO"));
    auto *ccSplitUTXOTi = new BlocknetLineEdit;
    changeSplitBoxLayout->addWidget(ccSplitUTXOCb);
    changeSplitBoxLayout->addWidget(ccSplitUTXOTi);

    auto *ccTitleBox = new QFrame;
    ccTitleBox->setLayout(new QVBoxLayout);
    ccTitleBox->layout()->setContentsMargins(QMargins());
    ccTitleBox->layout()->setSpacing(0);
    ccTitleBox->layout()->addWidget(ccTitleLbl);
    ccTitleBox->layout()->addWidget(ccSubtitleLbl);

    auto *ccManualHDiv = new BlocknetHDiv;

    auto *ccListTreeBox = new QFrame;
    auto *ccListTreeBoxLayout = new QHBoxLayout;
    ccListTreeBoxLayout->setContentsMargins(QMargins());
    ccListTreeBoxLayout->setSpacing(40);
    ccListTreeBox->setLayout(ccListTreeBoxLayout);
    auto *listModeRb = new QRadioButton(tr("List mode"));
    auto *treeModeRb = new QRadioButton(tr("Tree mode"));
    ccListTreeBoxLayout->addWidget(listModeRb, 0, Qt::AlignLeft);
    ccListTreeBoxLayout->addWidget(treeModeRb, 0, Qt::AlignLeft);
    ccListTreeBoxLayout->addStretch(1);

    ccManualBoxLayout->addWidget(changeSplitBox);
    ccManualBoxLayout->addWidget(ccManualHDiv);
    ccManualBoxLayout->addWidget(ccListTreeBox);

    // Top-level coin control layout
    ccBoxLayout->addWidget(ccTitleBox, 0, Qt::AlignLeft);
    ccBoxLayout->addWidget(ccDefaultRb, 0, Qt::AlignLeft);
    ccBoxLayout->addWidget(ccManualRb, 0, Qt::AlignLeft);
    ccBoxLayout->addWidget(ccManualBox);

    auto *changeBox = new QFrame;
    changeBox->setObjectName("changeSection");
    auto *changeBoxLayout = new QVBoxLayout;
    changeBoxLayout->setContentsMargins(QMargins());
    changeBoxLayout->setSpacing(0);
    changeBox->setLayout(changeBoxLayout);
    auto *changeAddrLbl = new QLabel(tr("Custom Change Address (optional)"));
    changeAddrLbl->setObjectName("sectionTitle");
    auto *changeAddrSubtitleLbl = new QLabel(tr("This will optionally put change back into a specific address of your choosing."));
    changeAddrSubtitleLbl->setObjectName("sectionSubtitle");
    changeAddrTi = new BlocknetLineEdit(350);
    changeAddrTi->setObjectName("changeAddress");
    changeAddrTi->setPlaceholderText(tr("Enter change address..."));
    changeBoxLayout->addWidget(changeAddrLbl);
    changeBoxLayout->addSpacing(2);
    changeBoxLayout->addWidget(changeAddrSubtitleLbl);
    changeBoxLayout->addSpacing(15);
    changeBoxLayout->addWidget(changeAddrTi);
    ccBoxLayout->addWidget(changeBox);

    // Cancel/continue buttons
    auto *btnBox = new QFrame;
    btnBox->setObjectName("buttonSection");
    auto *btnBoxLayout = new QHBoxLayout;
    btnBoxLayout->setContentsMargins(QMargins());
    btnBoxLayout->setSpacing(15);
    btnBox->setLayout(btnBoxLayout);
    btnBoxLayout->addWidget(cancelBtn, 0, Qt::AlignLeft);
    btnBoxLayout->addWidget(continueBtn, 0, Qt::AlignLeft);
    btnBoxLayout->addStretch(1);

    auto *hdiv1 = new BlocknetHDiv;
    auto *hdiv2 = new BlocknetHDiv;
    auto *hdiv3 = new BlocknetHDiv;

    layout->addWidget(titleLbl, 0, Qt::AlignTop | Qt::AlignLeft);
    layout->addSpacing(45);
    contentLayout->addWidget(subtitleLbl, 0, Qt::AlignTop);
    contentLayout->addSpacing(15);
    contentLayout->addWidget(fundList, 0);
    contentLayout->addSpacing(30);
    contentLayout->addWidget(hdiv1);
    contentLayout->addWidget(ccBox);
    contentLayout->addWidget(hdiv2);
    contentLayout->addWidget(changeBox);
    contentLayout->addWidget(hdiv3);
    contentLayout->addWidget(btnBox);
    contentLayout->addStretch(1);
    layout->addWidget(scrollArea, 0);

    connect(ccDefaultRb, SIGNAL(toggled(bool)), this, SLOT(onCoinControl()));
    connect(continueBtn, SIGNAL(clicked()), this, SLOT(onNext()));
    connect(cancelBtn, SIGNAL(clicked()), this, SLOT(onCancel()));
    connect(changeAddrTi, SIGNAL(editingFinished()), this, SLOT(onChangeAddress()));

    ccDefaultRb->setChecked(true);
}

void BlocknetSendFunds2::setData(BlocknetSendFundsModel *model) {
    BlocknetSendFundsPage::setData(model);
    displayUnit = walletModel->getOptionsModel()->getDisplayUnit();

    clear();
    bFundList = new BlocknetSendFunds2List(displayUnit);
    changeAddrTi->setText(model->changeAddress);

    uint i = 0;
    for (const BlocknetTransaction &r : model->recipients) {
        bFundList->addRow(i, r.address, r.amount > 0 ? r.getAmount(displayUnit) : QString());
        ++i;
    }

    fundList->layout()->addWidget(bFundList);
    fundList->adjustSize();
    contentLayout->update();
    layout->update();

    connect(bFundList, SIGNAL(amount(QString, QString)), this, SLOT(onAmount(QString, QString)));
}

/**
 * @brief Clears the existing visual elements from the display.
 */
void BlocknetSendFunds2::clear() {
    if (bFundList != nullptr)
        bFundList->clear();
    bFundList->deleteLater();
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
    changeAddrTi->clear();
}

/**
 * @brief  Validation check returns true if the minimum required data has been properly entered on the screen.
 * @return
 */
bool BlocknetSendFunds2::validated() {
    // Check recipients
    auto list = model->recipients.toList();
    bool allTxsValid = [](QList<BlocknetTransaction> &txs, WalletModel *w, int unit) -> bool {
        for (BlocknetTransaction &tx : txs) {
            if (!tx.isValid(w, unit))
                return false;
        }
        return true;
    }(list, walletModel, displayUnit);
    if (list.isEmpty() || !allTxsValid) {
        QString dust = BitcoinUnits::formatWithUnit(displayUnit, ::minRelayTxFee.GetFeePerK());
        auto msg = list.count() > 0 ? tr("Please specify send amounts larger than %1 for each address.").arg(dust) :
            tr("Please specify an amount larger than %1").arg(dust);
        QMessageBox::warning(this->parentWidget(), tr("Issue"), msg);
        return false;
    }

    // Check change address
    if (!changeAddrTi->text().isEmpty() && !walletModel->validateAddress(changeAddrTi->text())) {
        QMessageBox::warning(this->parentWidget(), tr("Issue"), QString("The change address is not valid address."));
        return false;
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

/**
 * @brief Sets the visible state of the manual coin control options.
 */
void BlocknetSendFunds2::onCoinControl() {
    ccManualBox->setHidden(ccDefaultRb->isChecked());
}

/**
 * @brief Assigns the change address to the fund model if it's valid.
 */
void BlocknetSendFunds2::onChangeAddress() {
    if (!changeAddrTi->text().isEmpty() && walletModel->validateAddress(changeAddrTi->text()))
        this->model->changeAddress = changeAddrTi->text();
}

/**
 * @brief Handles the amount signal from the send funds list. This method will convert the user specified amount
 *        into a double with a precision of 8. The max supported precision for Blocknet is 1/100000000. This
 *        method will mutate the transaction list with the new amount.
 * @param addr Address of the transaction
 * @param amount Amount of the transaction
 */
void BlocknetSendFunds2::onAmount(const QString addr, const QString amount) {
    CAmount newAmount = BlocknetTransaction::stringToInt(amount, displayUnit);

    // find recipient
    auto recipient = [](QList<BlocknetTransaction> list, const QString &address) -> BlocknetTransaction {
        for (BlocknetTransaction &t : list)
            if (t.address == address)
                return t;
        return BlocknetTransaction();
    }(model->recipients.toList(), addr);

    recipient.address = addr;
    recipient.amount = newAmount;

    // Replace existing transaction in the set, added if it doesn't already exist
    model->replaceRecipient(recipient);
}
