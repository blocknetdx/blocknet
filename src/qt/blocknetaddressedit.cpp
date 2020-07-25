// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/blocknetaddressedit.h>

#include <qt/blocknetguiutil.h>
#include <qt/blocknethdiv.h>

#include <qt/guiutil.h>

#include <key_io.h>
#include <util/system.h>
#include <wallet/wallet.h>

#include <QDoubleValidator>
#include <QKeyEvent>
#include <QMessageBox>
#include <QtWidgets>

BlocknetAddressEditDialog::BlocknetAddressEditDialog(AddressTableModel *model, WalletModel *walletModel,
        Qt::WindowFlags f, QWidget *parent) : QDialog(parent, f), model(model), walletModel(walletModel)
{
    this->setModal(true);
    this->setMinimumSize(BGU::spi(460), BGU::spi(410));
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
    // Edit address
    const auto dest = DecodeDestination(form->getAddress().toStdString());
    if (walletModel->wallet().getAddress(dest, nullptr, nullptr, nullptr))
        walletModel->wallet().setAddressBook(dest, form->getAlias().toStdString(), ""); // don't change purpose
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
    this->setMinimumSize(BGU::spi(460), BGU::spi(470));
    this->setStyleSheet(GUIUtil::loadStyleSheet());
    this->setContentsMargins(QMargins());
    this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    this->setWindowTitle(tr("Address Book"));
    form = new BlocknetAddressEdit(false, tr("New Address"), tr("Add Address"), this);
    CPubKey pubkey;
    if (walletModel->wallet().canGetAddresses() && walletModel->wallet().getKeyFromPool(false, pubkey))
        form->setNewAddress(GetDestinationForKey(pubkey, OutputType::LEGACY), pubkey);
    else
        QMessageBox::warning(this, tr("Wallet was unable to generate a new address"), tr("Try using getnewaddress command in Tools -> Debug Console"));
    form->show();
    connect(form, &BlocknetAddressEdit::accept, this, &QDialog::accept);
    connect(form, &BlocknetAddressEdit::cancel, this, &QDialog::reject);
}

void BlocknetAddressAddDialog::accept() {
    if (!form->getKey().isEmpty()) { // if private key field is not empty, we are "adding new address"
        auto import = [&]() -> bool {
            CKey secret = DecodeSecret(form->getKey().toStdString());
            if (!secret.IsValid()) {
                QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Bad private key"));
                return false;
            }
            return importPrivateKey(secret, form->getAlias(), form->getAddress());
        };
        // Request to unlock the wallet
        auto encStatus = walletModel->getEncryptionStatus();
        if (encStatus == walletModel->Locked || util::unlockedForStakingOnly) {
            WalletModel::UnlockContext ctx(walletModel->requestUnlock());
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
    } else {
        if (form->getAddress().isEmpty()) {
            QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Address cannot be blank"));
            return;
        }
        if (!IsValidDestinationString(form->getAddress().toStdString())) {
            QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Bad or invalid address, please review"));
            return;
        }

        // add send or receive address
        if (form->getType() == AddressTableModel::Send) {
            const auto dest = DecodeDestination(form->getAddress().toStdString());
            walletModel->wallet().setAddressBook(dest, form->getAlias().toStdString(), "send");
        } else if (form->getType() == AddressTableModel::Receive) {
            CTxDestination dest;
            auto pubkey = form->getPubKey();
            if (pubkey.IsFullyValid()) {
                walletModel->wallet().learnRelatedScripts(pubkey, OutputType::LEGACY);
                dest = GetDestinationForKey(pubkey, OutputType::LEGACY);
            } else
                dest = DecodeDestination(form->getAddress().toStdString());
            walletModel->wallet().setAddressBook(dest, form->getAlias().toStdString(), "receive");
        }
    }

    QDialog::accept();
}

bool BlocknetAddressAddDialog::importPrivateKey(CKey & key, const QString & alias, const QString & addr) {
    auto wallets = GetWallets();
    CWallet *pwallet = nullptr;
    for (auto & w : wallets) {
        if (walletModel->wallet().getWalletName() == w->GetName()) {
            pwallet = w.get();
            break;
        }
    }

    if (!pwallet) {
        QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Couldn't find the wallet"));
        return false;
    }

    if (pwallet->IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS)) {
        QMessageBox::warning(this->parentWidget(), tr("Issue"),
                tr("Cannot import private keys to a wallet with private keys disabled"));
        return false;
    }

    WalletRescanReserver reserver(pwallet);
    {
        auto locked_chain = pwallet->chain().lock();
        LOCK(pwallet->cs_wallet);

        if (pwallet->IsLocked()) {
            QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Wallet is locked, please unlock the wallet"));
            return false;
        }

        if (!key.IsValid()) {
            QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Bad private key"));
            return false;
        }

        CPubKey pubkey = key.GetPubKey();
        if (!key.VerifyPubKey(pubkey)) {
            QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Incompatible private key"));
            return false;
        }

        {
            pwallet->MarkDirty();

            // Check all pubkey addresses for the one specified in the form.
            bool found{false};
            for (const auto& dest : GetAllDestinationsForKey(pubkey)) {
                if (EncodeDestination(dest) == addr.toStdString()) {
                    found = true;
                    pwallet->DelAddressBook(dest);
                    pwallet->SetAddressBook(dest, alias.toStdString(), "receive");
                    break;
                }
            }

            if (!found) {
                QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Failed to add the key to the wallet"));
                return false;
            }

            const auto vchAddress = key.GetPubKey().GetID();
            if (pwallet->HaveKey(vchAddress))
                return true; // if address already exists return true

            // whenever a key is imported, we need to scan the whole chain
            pwallet->UpdateTimeFirstKey(1);
            pwallet->mapKeyMetadata[vchAddress].nCreateTime = 1;

            if (!pwallet->AddKeyPubKey(key, pubkey)) {
                QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Failed to add public key to wallet"));
                return false;
            }

            pwallet->LearnAllRelatedScripts(pubkey);

            // Prompt user about rescan
            QMessageBox::StandardButton retval = QMessageBox::question(this->parentWidget(), tr("Rescan the wallet"),
                                                              tr("You imported a new wallet address. Would you like to rescan the blockchain to add "
                                                                 "coin associated with this address? If you don't rescan, you may not see all your "
                                                                 "coin.\n\nThis may take several minutes."),
                                                              QMessageBox::Yes | QMessageBox::No,
                                                              QMessageBox::No);

            if (retval == QMessageBox::Yes)
                Q_EMIT rescan(pwallet->GetName());
        }
    }

    return true;
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
    layout->setContentsMargins(BGU::spi(25), BGU::spi(25), BGU::spi(25), BGU::spi(25));
    layout->setSpacing(0);

    titleLbl = new QLabel(title);
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
    layout->addSpacing(BGU::spi(20));
    layout->addWidget(addressTi);
    if (!editMode) {
        layout->addSpacing(BGU::spi(20));
        layout->addWidget(createAddressTi);
    }
    layout->addSpacing(BGU::spi(20));
    layout->addWidget(aliasTi);
    if (!editMode) {
        layout->addSpacing(BGU::spi(25));
        layout->addWidget(radioGrid, 0, Qt::AlignCenter);
    }
    layout->addSpacing(BGU::spi(25));
    layout->addWidget(div1);
    layout->addSpacing(BGU::spi(20));
    layout->addStretch(1);
    layout->addWidget(buttonGrid, 0, Qt::AlignCenter);

    connect(addressTi->lineEdit, &BlocknetLineEdit::textEdited, this, &BlocknetAddressEdit::onAddressChanged);
    connect(confirmBtn, &BlocknetFormBtn::clicked, this, &BlocknetAddressEdit::onApply);
    connect(cancelBtn, &BlocknetFormBtn::clicked, this, &BlocknetAddressEdit::onCancel);
    connect(otherUserBtn, &QRadioButton::toggled, this, &BlocknetAddressEdit::onOtherUser);
    if (!editMode)
        connect(createAddressTi->lineEdit, &BlocknetLineEdit::textEdited, this, &BlocknetAddressEdit::onPrivateKey);
}

QSize BlocknetAddressEdit::sizeHint() const {
    return { addressTi->width() + BGU::spi(60), 7 * BGU::spi(50) };
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
    if (!key.isEmpty()) {
        auto secret = DecodeSecret(key.toStdString());
        if (secret.IsValid())
            pubkey = secret.GetPubKey();
    } else
        pubkey = CPubKey();
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

QString BlocknetAddressEdit::getType() {
    if (myAddressBtn->isChecked())
        return AddressTableModel::Receive;
    return AddressTableModel::Send;
}

CPubKey BlocknetAddressEdit::getPubKey() {
    CTxDestination dest1 = GetDestinationForKey(pubkey, OutputType::LEGACY);
    CTxDestination dest2 = DecodeDestination(getAddress().toStdString());
    if (dest1 == dest2)
        return pubkey;
    else
        return CPubKey();
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
        return;
    }
    CKey secret = DecodeSecret(createAddressTi->lineEdit->text().toStdString());
    auto err = !secret.IsValid();
    createAddressTi->setError(err);
    if (!err) {
        const auto dest = GetDestinationForKey(secret.GetPubKey(), OutputType::LEGACY);
        pubkey = secret.GetPubKey();
        addressTi->lineEdit->setText(QString::fromStdString(EncodeDestination(dest)));
        myAddressBtn->setChecked(true);
        otherUserBtn->setChecked(false);
        otherUserBtn->setEnabled(false);
    } else { // on error
        addressTi->lineEdit->clear();
    }
}

void BlocknetAddressEdit::onAddressChanged(const QString &) {
    // The first time the screen is opened a new address is displayed (pulled from keystore)
    // As a result, if this value is mutated, we need to update the address description
    // so the user isn't confused about when the address is a auto-generated one.
    auto addrTitle = tr("Address");
    addressTi->setTitle(addrTitle);
}

bool BlocknetAddressEdit::validated() {
    if (addressTi->isEmpty()) {
        QMessageBox::warning(this->parentWidget(), tr("Issue"), tr("Please specify an address"));
        return false;
    }
    if (!editMode && !createAddressTi->isEmpty()) {
        CKey secret = DecodeSecret(createAddressTi->lineEdit->text().toStdString());
        auto err = !secret.IsValid();
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
    Q_EMIT accept();
}

void BlocknetAddressEdit::onOtherUser(bool checked) {
    if (editMode)
        return;
    if (otherUserBtn->isChecked())
        onAddressChanged(QString());
    createAddressTi->setEnabled(!otherUserBtn->isChecked());
}

bool BlocknetAddressEdit::setNewAddress(const CTxDestination & dest, const CPubKey & pkey) {
    if (!IsValidDestination(dest))
        return false;
    pubkey = pkey;
    myAddressBtn->setChecked(true);
    addressTi->lineEdit->setText(QString::fromStdString(EncodeDestination(dest)));
    return true;
}