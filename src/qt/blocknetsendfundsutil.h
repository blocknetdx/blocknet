// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETSENDFUNDSUTIL_H
#define BLOCKNETSENDFUNDSUTIL_H

#include "blocknetvars.h"

#include "bitcoinunits.h"
#include "walletmodel.h"
#include "coincontrol.h"
#include "amount.h"
#include "guiutil.h"

#include <QFrame>
#include <QSet>
#include <QString>
#include <QHash>
#include <sstream>
#include <iomanip>

struct BlocknetTransaction {
    QString address;
    CAmount amount{0};
    QString alias;
    explicit BlocknetTransaction(const QString address = QString(), const CAmount amount = 0,
            const QString alias = QString()) : address(address), amount(amount), alias(alias) {}
    bool operator==(const BlocknetTransaction &other) const {
        return address == other.address;
    }
    QString getAmount(int unit) const {
        return BitcoinUnits::format(unit, amount);
    }
    bool isValid(WalletModel *w, int unit) {
        // address is valid and amount is greater than dust
        return !address.isEmpty() && w->validateAddress(address) && amount > 0 &&
               !GUIUtil::isDust(address, CAmount(amount * BitcoinUnits::factor(unit)));
    }
    static CAmount stringToInt(const QString &str, int unit) {
        std::stringstream ss;
        ss << std::setprecision(BitcoinUnits::decimals(unit)) << std::fixed << str.toStdString();
        double r; ss >> r;
        return doubleToInt(r, unit);
    }
    static QString intToString(const CAmount a, int unit) {
        return BitcoinUnits::format(unit, a);
    }
    static CAmount doubleToInt(const double a, int unit) {
        return static_cast<CAmount>(a * BitcoinUnits::factor(unit));
    }
    static double intToDouble(const CAmount a, int unit) {
        std::stringstream ss;
        ss << std::setprecision(BitcoinUnits::decimals(unit)) << std::fixed <<
           static_cast<double>(a)/static_cast<double>(BitcoinUnits::factor(unit));
        double r; ss >> r;
        return r;
    }
    static QString doubleToString(double a, int unit) {
        std::stringstream ss;
        ss << std::setprecision(BitcoinUnits::decimals(unit)) << std::fixed << a;
        return QString::fromStdString(ss.str());
    }
    static double stringToDouble(const QString &str, int unit) {
        std::stringstream ss;
        ss << std::setprecision(BitcoinUnits::decimals(unit)) << std::fixed << str.toStdString();
        double r; ss >> r;
        return r;
    }
}; Q_DECLARE_METATYPE(BlocknetTransaction)

inline uint qHash(const BlocknetTransaction &t) {
    return qHash(t.address);
}

struct BlocknetSendFundsModel {
    QSet<BlocknetTransaction> recipients;
    QString changeAddress;
    bool customFee;
    CAmount userFee;

    CAmount txFees;
    CAmount txAmount;
    QList<SendCoinsRecipient> txRecipients;
    WalletModel::SendCoinsReturn txStatus;

    explicit BlocknetSendFundsModel() : changeAddress(QString()), customFee(false), userFee(0),
                                        txFees(0), txAmount(0), recipients(QSet<BlocknetTransaction>()),
                                        txRecipients(QList<SendCoinsRecipient>()) {};

    void addRecipient(const BlocknetTransaction &recipient) {
        recipients.insert(recipient);
    }
    void addRecipient(const QString &address, const CAmount amount, const QString alias = QString()) {
        recipients.insert(BlocknetTransaction{ address, amount, alias });
    }
    void removeRecipient(const BlocknetTransaction &recipient) {
        recipients.remove(recipient);
    }
    bool hasRecipient(const BlocknetTransaction &recipient) {
        return recipients.contains(recipient);
    }
    bool replaceRecipient(const BlocknetTransaction &recipient) {
        recipients.remove(recipient);
        recipients.insert(recipient);
        return true;
    }
    CAmount txTotalAmount() {
        return txAmount + txActiveFee();
    }
    CAmount txActiveFee() {
        return customFee ? userFee : txFees;
    }
    bool isZeroFee() {
        return txActiveFee() == 0;
    }

    void reset() {
        changeAddress.clear();
        customFee = false;
        userFee = 0;
        txFees = 0;
        txAmount = 0;
        recipients.clear();
        txRecipients.clear();
        txStatus = WalletModel::SendCoinsReturn();
    }

    CCoinControl getCoinControl(WalletModel *walletModel) {
        CCoinControl coinControl;

        if (walletModel->validateAddress(changeAddress)) {
            CBitcoinAddress addr(changeAddress.toStdString());
            if (addr.IsValid())
                coinControl.destChange     = addr.Get();
        }

        coinControl.fAllowOtherInputs      = true;
        coinControl.fOverrideFeeRate       = customFee;
        if (customFee)
            coinControl.nMinimumTotalFee   = userFee;
        coinControl.fAllowZeroValueOutputs = false;

        return coinControl;
    }

    WalletModel::SendCoinsReturn processFunds(WalletModel *walletModel, CCoinControl *coinControl) {
        txRecipients.clear();
        for (const BlocknetTransaction &tx : recipients) {
            SendCoinsRecipient recipient;
            recipient.address = tx.address;
            recipient.amount = tx.amount;
            recipient.useSwiftTX = false;
            recipient.inputType = ALL_COINS;
            txRecipients << recipient;
        }

        if (txRecipients.isEmpty()) {
            txStatus = WalletModel::InvalidAddress;
            return txStatus;
        }

        CAmount payFee = 0;
        if (coinControl && coinControl->fOverrideFeeRate)
            payFee = coinControl->nMinimumTotalFee;

        WalletModelTransaction walletTx(txRecipients);
        if (walletModel->isWalletLocked())
            txStatus = walletModel->prepareTransaction(walletTx, coinControl, payFee, false);
        else
            txStatus = walletModel->prepareTransaction(walletTx, coinControl, payFee);

        if (txStatus.status == WalletModel::OK || txStatus.status == WalletModel::Cancel) {
            txFees = walletTx.getTransactionFee();
            txAmount = walletTx.getTotalTransactionAmount();
        } else {
            txFees = 0;
            txAmount = 0;
        }

        return txStatus;
    }

}; Q_DECLARE_METATYPE(BlocknetSendFundsModel)

class BlocknetSendFundsPage : public QFrame {
    Q_OBJECT
public:
    explicit BlocknetSendFundsPage(WalletModel *w, int id, QFrame *parent = nullptr) : QFrame(parent), walletModel(w), pageID(id) { }
    void setWalletModel(WalletModel *w) { walletModel = w; }
    virtual void setData(BlocknetSendFundsModel *model) { this->model = model; }
    virtual void clear() {};
    virtual bool validated() = 0;

signals:
    void next(int pageID);
    void back(int pageID);
    void cancel(int pageID);

public slots:
    void onNext() { emit next(pageID); }
    void onBack() { emit back(pageID); }
    void onCancel() { emit cancel(pageID); }

protected:
    WalletModel *walletModel;
    int pageID{0};
    BlocknetSendFundsModel *model = nullptr;
};

#endif //BLOCKNETSENDFUNDSUTIL_H
