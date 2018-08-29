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

/**
 * @brief Blocknet transaction data object.
 */
struct BlocknetTransaction {
    QString address;
    CAmount amount{0};
    CAmount fee{0};
    QString alias;
    explicit BlocknetTransaction(const QString address = QString(), const CAmount amount = 0, const CAmount fee = 0,
            const QString alias = QString()) : address(address), amount(amount), fee(fee), alias(alias) {}
    bool operator==(const BlocknetTransaction &other) const {
        return address == other.address;
    }
    QString getAmount(int unit) const {
        return BitcoinUnits::format(unit, amount);
    }
    QString getAmountAfterFee(int unit) const {
        return BitcoinUnits::format(unit, amount - fee);
    }
    bool isValid(WalletModel *w) {
        // address is valid and amount is greater than dust
        return !address.isEmpty() && w->validateAddress(address) && amount > 0 &&
               !GUIUtil::isDust(address, amount);
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
        double aa = a + static_cast<double>(1) / static_cast<double>(BitcoinUnits::factor(unit) * 10);
        auto r = static_cast<CAmount>(aa * BitcoinUnits::factor(unit));
        return r;
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

/**
 * @brief Simple UTXO object used by the Blocknet data model. Compatible with Qt containers.
 */
struct BlocknetSimpleUTXO {
    uint256 hash;
    uint vout{0};
    QString address;
    CAmount amount{0};
    explicit BlocknetSimpleUTXO(const uint256 hash = uint256(), const uint vout = 0, const QString address = QString(), const CAmount amount = 0)
        : hash(hash), vout(vout), address(address), amount(amount) {}
    bool operator==(const BlocknetSimpleUTXO &other) const {
        return hash.GetHex() == other.hash.GetHex();
    }
    COutPoint outpoint() {
        COutPoint o(hash, static_cast<const uint32_t>(vout));
        return o;
    }
}; Q_DECLARE_METATYPE(BlocknetSimpleUTXO)

inline uint qHash(const BlocknetSimpleUTXO &t) {
    return qHash(QString::fromStdString(t.hash.GetHex()));
}

/**
 * @brief Manages the state through the Blocknet payment screens. This model is also responsible for creating any
 *        required wallet specific data for use with internal wallet functions.
 */
struct BlocknetSendFundsModel {
    QString changeAddress;
    bool customFee;
    CAmount userFee;
    CAmount txFees;
    CAmount txAmount;
    QSet<BlocknetTransaction> recipients;
    QList<SendCoinsRecipient> txRecipients;
    QVector<BlocknetSimpleUTXO> txSelectedUtxos;
    WalletModel::SendCoinsReturn txStatus;
    bool split;
    int splitCount;
    bool subtractFee;

    explicit BlocknetSendFundsModel() : changeAddress(QString()), customFee(false), userFee(0),
                                        txFees(0), txAmount(0), recipients(QSet<BlocknetTransaction>()),
                                        txRecipients(QList<SendCoinsRecipient>()), txSelectedUtxos(QVector<BlocknetSimpleUTXO>()),
                                        txStatus(WalletModel::SendCoinsReturn()), split(false), splitCount(0), subtractFee(false) {};

    void addRecipient(const BlocknetTransaction &recipient) {
        recipients.insert(recipient);
    }
    void addRecipient(const QString &address, const CAmount amount, const QString alias = QString()) {
        recipients.insert(BlocknetTransaction{ address, amount, 0, alias });
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
    CAmount totalRecipientsAmount() {
        CAmount t = 0;
        for (auto &r : recipients)
            t += r.amount;
        return t;
    }
    CAmount txTotalAmount() {
        return txAmount + txActiveFee();
    }
    CAmount txActiveFee() {
        return txFees;
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
        txSelectedUtxos.clear();
        txStatus = WalletModel::SendCoinsReturn();
        split = false;
        splitCount = 0;
        subtractFee = false;
    }

    void updateFees(QList<SendCoinsRecipient> &recs) {
        if (recipients.count() <= 0)
            return;

        // reset fees to 0
        auto list = recipients.toList();
        for (auto &t : list) {
            if (t.fee > 0)
                replaceRecipient(BlocknetTransaction{t.address, t.amount, 0, t.alias});
        }

        if (isZeroFee()) // do not assign estimated fees if 0 fee is specified
            return;

        for (auto &r : recs) {
            for (auto &t : recipients) {
                if (r.subtractFee && r.address == t.address) {
                    replaceRecipient(BlocknetTransaction{t.address, t.amount, t.amount - r.amount, t.alias});
                    break;
                }
            }
        }
    }

    CCoinControl getCoinControl(WalletModel *walletModel) {
        CCoinControl coinControl;

        if (walletModel->validateAddress(changeAddress)) {
            CBitcoinAddress addr(changeAddress.toStdString());
            if (addr.IsValid())
                coinControl.destChange     = addr.Get();
        }

        coinControl.fOverrideFeeRate       = customFee;
        if (customFee)
            coinControl.nMinimumTotalFee   = userFee;
        coinControl.fAllowZeroValueOutputs = false;
        coinControl.fAllowWatchOnly        = false;
        for (auto &o : txSelectedUtxos)
            coinControl.Select(o.outpoint());
        coinControl.fAllowOtherInputs      = !coinControl.HasSelected();
        coinControl.fSplitBlock            = split;
        coinControl.nSplitBlock            = split ? splitCount : 0;
        coinControl.fSubtractFee           = subtractFee;

        return coinControl;
    }

    /**
     * @brief Calculates the fees
     * @param walletModel
     * @param coinControl
     */
    WalletModel::SendCoinsReturn prepareFunds(WalletModel *walletModel, CCoinControl *coinControl) {
        txRecipients.clear();
        for (const BlocknetTransaction &tx : recipients) {
            SendCoinsRecipient recipient;
            recipient.address = tx.address;
            recipient.amount = tx.amount;
            recipient.useSwiftTX = false;
            recipient.inputType = ALL_COINS;
            recipient.subtractFee = coinControl ? coinControl->fSubtractFee : false;
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

        // Determine the total to recipients not including change
        CAmount totalAmount = 0;
        for (const auto &r : txRecipients)
            totalAmount += r.amount;

        txFees = walletTx.getTransactionFee();
        txAmount = coinControl->fSubtractFee ? totalAmount - txFees : totalAmount;

        auto recs = walletTx.getRecipients();
        updateFees(recs);

        return txStatus;
    }

    /**
     * @brief Calculates the estimated fee based on the specified coin inputs.
     * @param walletModel
     * @param coinControl
     */
    WalletModel::SendCoinsReturn prepareFundsCoinInputs(WalletModel *walletModel, CCoinControl *coinControl) {
        QVector<WalletModel::CoinInput> inputs;
        for (auto &t : txSelectedUtxos)
            inputs.push_back({ t.hash, t.vout, t.address, t.amount });

        CAmount totalAmount = this->totalRecipientsAmount();
        auto res = walletModel->getFeeInfo(inputs, totalAmount);
        txFees = coinControl->fOverrideFeeRate ? coinControl->nMinimumTotalFee : res.fee;
        txAmount = coinControl->fSubtractFee ? totalAmount - txFees : totalAmount;

        if (res.change < 0) { // if not enough to cover transaction
            if (coinControl->fSubtractFee && res.change + res.fee >= 0)
                txStatus = WalletModel::OK;
            else txStatus = WalletModel::AmountWithFeeExceedsBalance;
        } else {
            txStatus = WalletModel::OK;
        }

        // Set recipients data
        txRecipients.clear();
        for (const BlocknetTransaction &tx : recipients) {
            SendCoinsRecipient recipient;
            recipient.address = tx.address;
            recipient.amount = tx.amount;
            recipient.useSwiftTX = false;
            recipient.inputType = ALL_COINS;
            recipient.subtractFee = coinControl ? coinControl->fSubtractFee : false;
            txRecipients << recipient;
        }

        QList<SendCoinsRecipient> recs;

        // Produce list of recipients with the fee subtracted from the totals (for use in calculating the fee amounts)
        if (coinControl && coinControl->fSubtractFee) {
            bool isFirst = true;
            for (const BlocknetTransaction &tx : recipients) {
                SendCoinsRecipient rec;
                rec.address = tx.address;
                if (coinControl && coinControl->fSubtractFee) { // subtract fee from amount equally across recipients
                    CAmount amount = tx.amount - txFees / recipients.count();
                    if (isFirst && recipients.count() > 1) { // store remainder on first recipient
                        amount -= txFees % recipients.count();
                        isFirst = false;
                    }
                    rec.amount = amount;
                } else rec.amount = tx.amount;
                rec.useSwiftTX = false;
                rec.inputType = ALL_COINS;
                rec.subtractFee = true;
                recs << rec;
            }
        }

        updateFees(recs);

        return txStatus;
    }

    /**
     * @brief  Returns the total amount of COIN selected by the coin control widget.
     * @return
     */
    CAmount selectedInputsTotal() const {
        CAmount total = 0;
        for (auto &t : txSelectedUtxos)
            total += t.amount;
        return total;
    }

    /**
     * @brief  Returns the selected COutPoint objects.
     * @return
     */
    std::vector<COutPoint> selectedOutpoints() const {
        std::vector<COutPoint> r;
        for (auto &t : txSelectedUtxos)
            r.emplace_back(t.hash, t.vout);
        return r;
    }

}; Q_DECLARE_METATYPE(BlocknetSendFundsModel)

/**
 * @brief Parent class responsible for the Send Funds screen flow.
 */
class BlocknetSendFundsPage : public QFrame {
    Q_OBJECT
public:
    explicit BlocknetSendFundsPage(WalletModel *w, int id, QFrame *parent = nullptr) : QFrame(parent), walletModel(w), pageID(id) { }
    void setWalletModel(WalletModel *w) { walletModel = w; }
    virtual void setData(BlocknetSendFundsModel *model) { clear(); this->model = model; }
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

/**
 * @brief Double validator, rejects separators.
 */
class BlocknetNumberValidator : public QDoubleValidator {
    Q_OBJECT
public:
    explicit BlocknetNumberValidator(double bottom, double top, int decimals, QObject *parent = 0) : QDoubleValidator(bottom, top, decimals, parent) {
        this->setLocale(QLocale::C);
    }
    QValidator::State validate(QString &input, int &pos) const override {
        if (input.contains(QChar(',')))
            return QValidator::Invalid;
        return QDoubleValidator::validate(input, pos);
    }
    void fixup(QString &input) const override {
        input.remove(QChar(','), Qt::CaseInsensitive);
        QDoubleValidator::fixup(input);
    }
};

#endif //BLOCKNETSENDFUNDSUTIL_H
