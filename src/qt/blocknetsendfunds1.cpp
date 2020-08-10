// Copyright (c) 2018-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetsendfunds1.h>

#include <qt/blocknetaddressbook.h>
#include <qt/blocknetguiutil.h>
#include <qt/blockneticonbtn.h>

#include <qt/addresstablemodel.h>

#include <QKeyEvent>
#include <QMessageBox>

BlocknetSendFunds1::BlocknetSendFunds1(WalletModel *w, int id, QFrame *parent) : BlocknetSendFundsPage(w, id, parent),
                                                                                 layout(new QVBoxLayout) {
//    this->setStyleSheet("border: 1px solid red");
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(layout);
    layout->setContentsMargins(BGU::spi(15), BGU::spi(10), BGU::spi(35), BGU::spi(30));

    titleLbl = new QLabel(tr("Send Funds"));
    titleLbl->setObjectName("h4");

    auto *subtitleLbl = new QLabel(tr("Who would you like to send funds to?"));
    subtitleLbl->setObjectName("h2");

    addressTi = new BlocknetAddressEditor(BGU::spi(675));
    addressTi->setPlaceholderText(tr("Enter Blocknet Address..."));
    addressTi->setAddressValidator([w](QString &addr) -> bool {
        if (w == nullptr)
            return false;
        else return w->validateAddress(addr);
    });

    auto *addAddressBtn = new BlocknetIconBtn(QString("Open Address Book"), ":/redesign/QuickActions/AddressBookIcon.png");

    continueBtn = new BlocknetFormBtn;
    continueBtn->setText(tr("Continue"));
    continueBtn->setDisabled(true);

    auto *hdiv = new QLabel;
    hdiv->setFixedHeight(1);
    hdiv->setObjectName("hdiv");

    layout->addWidget(titleLbl, 0, Qt::AlignTop | Qt::AlignLeft);
    layout->addSpacing(BGU::spi(30));
    layout->addWidget(subtitleLbl, 0, Qt::AlignTop);
    layout->addSpacing(BGU::spi(25));
    layout->addWidget(addressTi);
    layout->addSpacing(BGU::spi(20));
    layout->addWidget(addAddressBtn, 0, Qt::AlignLeft);
    layout->addSpacing(BGU::spi(35));
    layout->addWidget(hdiv);
    layout->addSpacing(BGU::spi(40));
    layout->addWidget(continueBtn);
    layout->addStretch(1);

    connect(addressTi, &BlocknetAddressEditor::textChanged, this, &BlocknetSendFunds1::textChanged);
    connect(addressTi, &BlocknetAddressEditor::addresses, this, &BlocknetSendFunds1::onAddressesChanged);
    connect(addressTi, &BlocknetAddressEditor::returnPressed, this, &BlocknetSendFunds1::onNext);
    connect(addAddressBtn, &BlocknetIconBtn::clicked, this, &BlocknetSendFunds1::openAddressBook);
    connect(continueBtn, &BlocknetFormBtn::clicked, this, &BlocknetSendFunds1::onNext);
}

void BlocknetSendFunds1::setData(BlocknetSendFundsModel *model) {
    BlocknetSendFundsPage::setData(model);
    addressTi->blockSignals(true);
    // Add addresses to display
    for (const SendCoinsRecipient & r : model->txRecipients())
        addressTi->addAddress(r.address);
    addressTi->blockSignals(false);
}

void BlocknetSendFunds1::addAddress(const QString &address) {
    addressTi->addAddress(address);
}

void BlocknetSendFunds1::openAddressBook() {
    BlocknetAddressBookDialog dlg(walletModel, Qt::WindowSystemMenuHint | Qt::WindowTitleHint);
    connect(&dlg, &BlocknetAddressBookDialog::send, [this](const QString &address) {
        addAddress(address);
    });
    dlg.exec();
}

void BlocknetSendFunds1::clear() {
    addressTi->blockSignals(true);
    addressTi->clearData();
    addressTi->blockSignals(false);
}

void BlocknetSendFunds1::focusInEvent(QFocusEvent *event) {
    QWidget::focusInEvent(event);
    addressTi->setFocus();
}

void BlocknetSendFunds1::keyPressEvent(QKeyEvent *event) {
    QWidget::keyPressEvent(event);
    if (this->isHidden())
        return;
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
        onNext();
}

void BlocknetSendFunds1::textChanged() {
    continueBtn->setDisabled(addressTi->getAddresses().isEmpty());
}

bool BlocknetSendFunds1::validated() {
    if (addressTi->getAddresses().isEmpty()) {
        QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Please add a valid address to the send box."));
        return false;
    }

    // Use wallet to validate address
    auto addresses = addressTi->getAddresses().toList();
    QString invalidAddresses;
    for (QString &addr : addresses) {
        if (!walletModel->validateAddress(addr))
            invalidAddresses += QString("\n%1").arg(addr);
    }

    // Display invalid addresses
    if (!invalidAddresses.isEmpty()) {
        QMessageBox::warning(this->parentWidget(), tr("Issue"),
                             QString("%1:\n%2").arg(tr("Please correct the invalid addresses below"), invalidAddresses));
        return false;
    }

    return true;
}

void BlocknetSendFunds1::onAddressesChanged() {
    // First add any new addresses (do not overwrite existing)
    auto addresses = addressTi->getAddresses();
    QSet<QString> setAddresses;
    for (const QString &addr : addresses) {
        setAddresses.insert(addr);
        if (!model->hasRecipient(addr))
            model->addRecipient(addr, 0, walletModel->getAddressTableModel()->labelForAddress(addr));
    }
    // Remove any unspecified addresses
    for (const auto & r : model->txRecipients())  {
        if (!setAddresses.contains(r.address))
            model->removeRecipient(r.address);
    }
}
