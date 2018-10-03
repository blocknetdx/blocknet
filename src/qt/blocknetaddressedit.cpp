// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blocknetaddressedit.h"
#include "blocknethdiv.h"

#include "init.h"
#include "wallet.h"
#include "guiutil.h"

#include <QMessageBox>
#include <QKeyEvent>
#include <QDoubleValidator>
#include <QtWidgets>

BlocknetAddressEditDialog::BlocknetAddressEditDialog(AddressTableModel *model, Qt::WindowFlags f, QWidget *parent) : QDialog(parent, f),
                                                                                                                     model(model) {
    this->setModal(true);
    this->setMinimumSize(460, 400);
    this->setStyleSheet(GUIUtil::loadStyleSheet());
    this->setContentsMargins(QMargins());
    this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    this->setWindowTitle(tr("Address Book"));
    form = new BlocknetAddressEdit(true, tr("Edit Address"), tr("Save"), this);
    form->show();
    connect(form, &BlocknetAddressEdit::accept, this, &QDialog::accept);
    connect(form, &BlocknetAddressEdit::cancel, this, &QDialog::reject);
}

void BlocknetAddressEditDialog::accept() {
    auto rows = model->rowCount(QModelIndex());
    for (int row = rows - 1; row >= 0; --row) {
        auto indexAddr = model->index(row, AddressTableModel::Address, QModelIndex());
        if (form->getAddress() == indexAddr.data(Qt::EditRole).toString()) {
            auto indexLabel = model->index(row, AddressTableModel::Label, QModelIndex());
            model->setData(indexAddr, form->getAddress(), Qt::EditRole);
            model->setData(indexLabel, form->getAlias(), Qt::EditRole);
            model->setData(indexLabel, form->getType() == AddressTableModel::Send ? AddressTableEntry::Sending : AddressTableEntry::Receiving,
                    AddressTableModel::TypeRole);
            model->invalidateLabel(form->getAddress());
            break;
        }
    }
    QDialog::accept();
}

void BlocknetAddressEditDialog::resizeEvent(QResizeEvent *evt) {
    QDialog::resizeEvent(evt);
    form->resize(evt->size().width(), evt->size().height());
}

void BlocknetAddressEditDialog::setData(const QString &address, const QString &alias, const int &type, const QString &key) {
    form->setData(address, alias, type, key);
}

BlocknetAddressAddDialog::BlocknetAddressAddDialog(AddressTableModel *model, WalletModel *walletModel,
        Qt::WindowFlags f, QWidget *parent) : QDialog(parent, f), model(model), walletModel(walletModel) {
    this->setModal(true);
    this->setMinimumSize(460, 480);
    this->setStyleSheet(GUIUtil::loadStyleSheet());
    this->setContentsMargins(QMargins());
    this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    this->setWindowTitle(tr("Address Book"));
    form = new BlocknetAddressEdit(false, tr("Add Address"), tr("Add Address"), this);
    form->show();
    connect(form, &BlocknetAddressEdit::accept, this, &QDialog::accept);
    connect(form, &BlocknetAddressEdit::cancel, this, &QDialog::reject);
}

void BlocknetAddressAddDialog::accept() {
    auto defaultAddress = QString::fromStdString(CBitcoinAddress(form->getKeyID()).ToString());
    if (!form->getKey().isEmpty()) {
        auto import = [&]() -> bool {
            CBitcoinSecret secret;
            if (!secret.SetString(form->getKey().toStdString())) {
                QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Bad private key"));
                return false;
            }

            CKey key = secret.GetKey();
            if (!key.IsValid()) {
                QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Bad private key"));
                return false;
            }

            CPubKey pubkey = key.GetPubKey();
            if (!key.VerifyPubKey(pubkey)) {
                QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("The import failed. The private key failed verification"));
                return false;
            }
            CKeyID vchAddress = pubkey.GetID();
            {
                pwalletMain->MarkDirty();
                pwalletMain->SetAddressBook(vchAddress, form->getAlias().toStdString(), "receive");

                // Don't throw error in case a key is already there
                if (pwalletMain->HaveKey(vchAddress))
                    return false;

                pwalletMain->mapKeyMetadata[vchAddress].nCreateTime = 1;
                if (!pwalletMain->AddKeyPubKey(key, pubkey)) // no rescan if this fails
                    return false;

                // whenever a key is imported, we need to scan the whole chain
                pwalletMain->nTimeFirstKey = 1; // 0 would be considered 'no value'
                return true;
            }
        };
        // Request to unlock the wallet
        auto encStatus = walletModel->getEncryptionStatus();
        if (encStatus == walletModel->Locked || encStatus == walletModel->UnlockedForAnonymizationOnly) {
            WalletModel::UnlockContext ctx(walletModel->requestUnlock(true));
            if (!ctx.isValid()) {
                // Unlock wallet was cancelled
                return;
            }
            if (import()) {
                QDialog::accept();
                return;
            }
        } else if (import()) {
            QDialog::accept();
            return;
        }
    }
    else if (form->getAddress() == defaultAddress && form->getKeyID() != CKeyID())
        pwalletMain->SetAddressBook(form->getKeyID(), form->getAlias().toStdString(), "receive");
    else model->addRow(form->getType(), form->getAlias(), form->getAddress());
    QDialog::accept();
}

void BlocknetAddressAddDialog::resizeEvent(QResizeEvent *evt) {
    QDialog::resizeEvent(evt);
    form->resize(evt->size().width(), evt->size().height());
}

BlocknetAddressEdit::BlocknetAddressEdit(bool editMode, const QString &t, const QString &b, QWidget *parent) : QFrame(parent),
                                                                                                               layout(new QVBoxLayout),
                                                                                                               title(t),
                                                                                                               buttonString(b),
                                                                                                               editMode(editMode) {
    this->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    this->setLayout(layout);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(0);

    QLabel *titleLbl = new QLabel(title);
    titleLbl->setObjectName("h2");

    auto addrTitle = editMode ? tr("Address") : tr("Address (auto-generated from your keystore)");
    addressTi = new BlocknetLineEditWithTitle(addrTitle, tr("Enter Address..."));
    addressTi->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    aliasTi = new BlocknetLineEditWithTitle(tr("Alias (optional)"), tr("Enter alias..."));
    aliasTi->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    if (!editMode) {
        createAddressTi = new BlocknetLineEditWithTitle(tr("Import Address from Private Key (debug console command: dumpprivkey)"), tr("Enter private key..."));
        createAddressTi->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
        createAddressTi->lineEdit->setEchoMode(QLineEdit::Password);
    }
    if (editMode) {
        addressTi->lineEdit->setObjectName("readOnly");
        addressTi->setEnabled(false);
    }

    auto *radioGrid = new QFrame;
    auto *radioLayout = new QHBoxLayout;
    radioLayout->setContentsMargins(QMargins());
    radioGrid->setLayout(radioLayout);
    myAddressBtn = new QRadioButton(tr("My Address"));
    otherUserBtn = new QRadioButton(tr("Other Contact"));
    otherUserBtn->setObjectName("otherUserBtn");
    otherUserBtn->setChecked(false);
    radioLayout->addWidget(myAddressBtn);
    radioLayout->addWidget(otherUserBtn);

    auto *div1 = new BlocknetHDiv;

    auto *buttonGrid = new QFrame;
    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->setContentsMargins(QMargins());
    buttonGrid->setLayout(buttonLayout);
    confirmBtn = new BlocknetFormBtn;
    confirmBtn->setText(buttonString);
    cancelBtn = new BlocknetFormBtn;
    cancelBtn->setObjectName("cancel");
    cancelBtn->setText(tr("Cancel"));
    buttonLayout->addWidget(cancelBtn);
    buttonLayout->addWidget(confirmBtn);

    layout->addWidget(titleLbl);
    layout->addSpacing(20);
    layout->addWidget(addressTi);
    if (!editMode) {
        layout->addSpacing(20);
        layout->addWidget(createAddressTi);
    }
    layout->addSpacing(20);
    layout->addWidget(aliasTi);
    if (!editMode) {
        layout->addSpacing(25);
        layout->addWidget(radioGrid, 0, Qt::AlignCenter);
    }
    layout->addSpacing(25);
    layout->addWidget(div1);
    layout->addSpacing(25);
    layout->addStretch(1);
    layout->addWidget(buttonGrid, 0, Qt::AlignCenter);

    connect(addressTi->lineEdit, SIGNAL(textEdited(const QString &)), this, SLOT(onAddressChanged(const QString &)));
    connect(confirmBtn, SIGNAL(clicked()), this, SLOT(onApply()));
    connect(cancelBtn, SIGNAL(clicked()), this, SLOT(onCancel()));
    connect(otherUserBtn, SIGNAL(toggled(bool)), this, SLOT(onOtherUser(bool)));
    if (!editMode)
        connect(createAddressTi->lineEdit, SIGNAL(textEdited(const QString &)), this, SLOT(onPrivateKey(const QString &)));

    // Generate new address
    if (!editMode) {
        QString newAddress;
        if (generateAddress(newAddress))
            addressTi->lineEdit->setText(newAddress);
        myAddressBtn->setChecked(true);
    }
}

QSize BlocknetAddressEdit::sizeHint() const {
    return { addressTi->width() + 60, 7 * 50 };
}

void BlocknetAddressEdit::setData(const QString &address, const QString &alias, const int &type, const QString &key) {
    addressTi->lineEdit->setText(address);
    aliasTi->lineEdit->setText(alias);
    myAddressBtn->setChecked(type == AddressTableEntry::Receiving);
    otherUserBtn->setChecked(type == AddressTableEntry::Sending);
    if (!editMode) {
        createAddressTi->lineEdit->setText(key);
        createAddressTi->setEnabled(!otherUserBtn->isChecked());
    }
}

QString BlocknetAddressEdit::getAddress() {
    return addressTi->lineEdit->text().trimmed();
}

QString BlocknetAddressEdit::getAlias() {
    return aliasTi->lineEdit->text().trimmed();
}

QString BlocknetAddressEdit::getKey() {
    if (!editMode)
        return createAddressTi->lineEdit->text().trimmed();
    return QString();
}

CKeyID BlocknetAddressEdit::getKeyID() {
    return keyID;
}

QString BlocknetAddressEdit::getType() {
    if (myAddressBtn->isChecked())
        return AddressTableModel::Receive;
    return AddressTableModel::Send;
}

void BlocknetAddressEdit::clear() {
    addressTi->lineEdit->clear();
    aliasTi->lineEdit->clear();
    if (!editMode)
        createAddressTi->lineEdit->clear();
}

void BlocknetAddressEdit::focusInEvent(QFocusEvent *event) {
    QWidget::focusInEvent(event);
    addressTi->setFocus();
}

void BlocknetAddressEdit::keyPressEvent(QKeyEvent *event) {
    QWidget::keyPressEvent(event);
    if (this->isHidden())
        return;
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
        onApply();
}

void BlocknetAddressEdit::onPrivateKey(const QString &) {
    if (createAddressTi->isEmpty()) {
        createAddressTi->setError(false);
        otherUserBtn->setEnabled(true);
        keyID = CKeyID();
        return;
    }
    CBitcoinSecret secret;
    auto err = !secret.SetString(createAddressTi->lineEdit->text().toStdString()) || !secret.IsValid();
    createAddressTi->setError(err);
    if (!err) {
        CBitcoinAddress address(secret.GetKey().GetPubKey().GetID());
        addressTi->lineEdit->setText(QString::fromStdString(address.ToString()));
        myAddressBtn->setChecked(true);
        otherUserBtn->setChecked(false);
        otherUserBtn->setEnabled(false);
    } else { // on error
        addressTi->lineEdit->clear();
        keyID = CKeyID();
    }
}

void BlocknetAddressEdit::onAddressChanged(const QString &) {
    // The first time the screen is opened a new address is displayed (pulled from keystore)
    // As a result, if this value is mutated, we need to update the address description
    // so the user isn't confused about when the address is a auto-generated one.
    auto addrTitle = tr("Address");
    addressTi->setTitle(addrTitle);
    keyID = CKeyID(); // also reset potential generated key id
}

bool BlocknetAddressEdit::validated() {
    if (addressTi->isEmpty()) {
        QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Please specify an address"));
        return false;
    }
    if (!editMode && !createAddressTi->isEmpty()) {
        CBitcoinSecret secret;
        auto err = !secret.SetString(createAddressTi->lineEdit->text().toStdString()) || !secret.IsValid();
        createAddressTi->setError(err);
        if (err) {
            QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Bad private key"));
            return false;
        }
    }
    return true;
}

void BlocknetAddressEdit::onApply() {
    if (!validated())
        return;
    emit accept();
}

void BlocknetAddressEdit::onOtherUser(bool checked) {
    if (editMode)
        return;
    if (otherUserBtn->isChecked())
        onAddressChanged(QString());
    createAddressTi->setEnabled(!otherUserBtn->isChecked());
}

bool BlocknetAddressEdit::generateAddress(QString &newAddress) {
    if (!pwalletMain->IsLocked())
        pwalletMain->TopUpKeyPool();

    // Generate a new key that is added to wallet
    CPubKey newKey;
    if (!pwalletMain->GetKeyFromPool(newKey))
        return false;

    newAddress = QString::fromStdString(CBitcoinAddress(newKey.GetID()).ToString());
    keyID = newKey.GetID();

    return true;
}