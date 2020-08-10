// Copyright (c) 2018-2020 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETSENDFUNDSUTIL_H
#define BLOCKNET_QT_BLOCKNETSENDFUNDSUTIL_H

#include <qt/blocknetvars.h>

#include <qt/bitcoinunits.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/walletmodel.h>

#include <amount.h>
#include <interfaces/node.h>
#include <key_io.h>
#include <policy/policy.h>
#include <wallet/coincontrol.h>

#include <iomanip>
#include <sstream>
#include <utility>

#include <QFrame>
#include <QHash>
#include <QSet>
#include <QString>

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

inline uint qHash(const BlocknetSimpleUTXO & t) {
    return qHash(QString::fromStdString(t.hash.GetHex()));
}

/**
 * @brief Manages the state through the Blocknet payment screens. This model is also responsible for creating any
 *        required wallet specific data for use with internal wallet functions.
 */
class BlocknetSendFundsModel {
public:
    explicit BlocknetSendFundsModel() : changeAddress_(QString()), customFee_(false), userFee_(0),
                                        split_(false), splitCount_(0), subtractFee_(false), txFees_(0),
                                        txRecipients_(QList<SendCoinsRecipient>()),
                                        txSelectedUtxos_(QVector<BlocknetSimpleUTXO>()),
                                        txStatus_(WalletModel::SendCoinsReturn()),
                                        walletTx_(nullptr) {};
    ~BlocknetSendFundsModel() {
        delete walletTx_;
        walletTx_ = nullptr;
    }

    static bool recipientEquals(const SendCoinsRecipient & r1, const SendCoinsRecipient & r2) {
        return r1.fSubtractFeeFromAmount == r2.fSubtractFeeFromAmount
               && r1.address == r2.address
               && r1.amount == r2.amount
               && r1.authenticatedMerchant == r2.authenticatedMerchant
               && r1.label == r2.label
               && r1.message == r2.message
               && r1.nVersion == r2.nVersion;
    }

    void addRecipient(const SendCoinsRecipient & recipient) {
        txRecipients_.push_back(recipient);
    }
    void addRecipient(const QString & address, const CAmount amount, const QString & alias) {
        txRecipients_.push_back(SendCoinsRecipient{ address, alias, amount, QString() });
    }
    bool hasRecipient(const QString & address) {
        for (const auto & r : txRecipients_)
            if (r.address == address)
                return true;
        return false;
    }
    void removeRecipient(const SendCoinsRecipient & recipient) {
        txRecipients_.erase(std::remove_if(txRecipients_.begin(), txRecipients_.end(),
                [&recipient,this](const SendCoinsRecipient & r) {
                    return recipientEquals(recipient, r);
                }), txRecipients_.end());
    }
    void removeRecipient(const QString & address) {
        txRecipients_.erase(std::remove_if(txRecipients_.begin(), txRecipients_.end(),
                [&address, this](const SendCoinsRecipient & r) {
                    return r.address == address;
                }), txRecipients_.end());
    }
    const QList<SendCoinsRecipient> & txRecipients() const {
        return txRecipients_;
    }

    CAmount totalRecipientsAmount() const {
        CAmount t = 0;
        for (auto & r : txRecipients_)
            t += r.amount;
        return t;
    }

    /**
     * Returns the total amount including the fees.
     * @return
     */
    CAmount txTotalAmount() const {
        return totalRecipientsAmount();
    }

    CAmount txFees() const {
        return txFees_;
    }

    CAmount txAmountMinusFee(const CAmount & amount) const {
        if (subtractFee_)
            return amount - txFees_/txRecipients_.size();
        return amount;
    }

    void setCustomFee(const bool flag) {
        customFee_ = flag;
    }
    bool customFee() const {
        return customFee_;
    }

    void setUserFee(const CAmount fee) {
        userFee_ = fee;
    }
    CAmount userFee() const {
        return userFee_;
    }

    void setSubtractFee(const bool flag) {
        subtractFee_ = flag;
        for (auto & r : txRecipients_)
            r.fSubtractFeeFromAmount = subtractFee_;
    }
    bool subtractFee() const {
        return subtractFee_;
    }

    WalletModel::SendCoinsReturn txStatus() const {
        return txStatus_;
    }

    bool hasWalletTx() const {
        return walletTx_ != nullptr;
    }
    WalletModelTransaction* walletTx() const {
        return walletTx_;
    }

    void setChangeAddress(const QString & addr) {
        changeAddress_ = addr;
    }
    QString changeAddress() const {
        return changeAddress_;
    }

    void setTxSelectedUtxos(const QVector<BlocknetSimpleUTXO> & utxos) {
        txSelectedUtxos_ = utxos;
    }
    const QVector<BlocknetSimpleUTXO> & txSelectedUtxos() const {
        return txSelectedUtxos_;
    }
    void clearTxSelectedUtxos() {
        txSelectedUtxos_.clear();
    }

    void setEstimatedFees(const CAmount fees) {
        estimatedFees_ = fees;
    }
    CAmount estimatedFees() const {
        return estimatedFees_;
    }

    void setSplit(const bool flag) {
        split_ = flag;
    }
    bool split() const {
        return split_;
    }
    void setSplitCount(const int count) {
        splitCount_ = count;
    }
    int splitCount() const {
        return splitCount_;
    }

    void reset() {
        changeAddress_.clear();
        customFee_ = false;
        subtractFee_ = false;
        userFee_ = 0;
        txFees_ = 0;
        txRecipients_.clear();
        txSelectedUtxos_.clear();
        txStatus_ = WalletModel::SendCoinsReturn();
        split_ = false;
        splitCount_ = 0;
        estimatedFees_ = 0;
        if (hasWalletTx()) {
            delete walletTx_;
            walletTx_ = nullptr;
        }
    }

    CCoinControl getCoinControl(WalletModel *walletModel) {
        CCoinControl coinControl;
        coinControl.fAllowWatchOnly = false;

        if (walletModel->validateAddress(changeAddress_))
            coinControl.destChange = DecodeDestination(changeAddress_.toStdString());

        for (auto &o : txSelectedUtxos_)
            coinControl.Select(o.outpoint());
        coinControl.fAllowOtherInputs = !coinControl.HasSelected();
        // TODO Blocknet Qt coin splitting
//        coinControl.fSplitBlock = split;
//        coinControl.nSplitBlock = split ? splitCount : 0;

        return coinControl;
    }

    /**
     * @brief Prepares the transaction from all known state.
     * @param walletModel
     * @param coinControl
     */
    WalletModel::SendCoinsReturn prepareFunds(WalletModel *walletModel, CCoinControl & coinControl) {
        if (txRecipients_.isEmpty()) {
            txStatus_ = WalletModel::InvalidAddress;
            return txStatus_;
        }

        coinControl = getCoinControl(walletModel);
        auto recipients = txRecipients_;
        for (auto & r : recipients)
            r.fSubtractFeeFromAmount = subtractFee_;
        if (customFee_)
            coinControl.m_total_fee = userFee_;

        if (hasWalletTx()) {
            delete walletTx_;
            walletTx_ = nullptr;
        }
        walletTx_ = new WalletModelTransaction(recipients);

        // Check that wallet is unlocked
        if (walletModel->getEncryptionStatus() == WalletModel::EncryptionStatus::Locked) {
            WalletModel::UnlockContext ctx(walletModel->requestUnlock());
            if (!ctx.isValid())
                return WalletModel::StatusCode::TransactionCreationFailed;
        }

        txStatus_ = walletModel->prepareTransaction(*walletTx_, coinControl);
        txFees_ = walletTx_->getTransactionFee();

        return txStatus_;
    }

    /**
     * @brief  Returns the total amount of COIN selected by the coin control widget.
     * @return
     */
    CAmount selectedInputsTotal() const {
        CAmount total = 0;
        for (auto & t : txSelectedUtxos_)
            total += t.amount;
        return total;
    }

    /**
     * @brief  Returns the selected COutPoint objects.
     * @return
     */
    std::vector<COutPoint> selectedOutpoints() const {
        std::vector<COutPoint> r;
        for (auto & t : txSelectedUtxos_)
            r.emplace_back(t.hash, t.vout);
        return r;
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

private:
    QString changeAddress_;
    bool customFee_;
    CAmount userFee_;
    bool subtractFee_;
    bool split_;
    int splitCount_;
    CAmount txFees_;
    CAmount estimatedFees_;
    QList<SendCoinsRecipient> txRecipients_;
    QVector<BlocknetSimpleUTXO> txSelectedUtxos_;
    WalletModel::SendCoinsReturn txStatus_;
    WalletModelTransaction *walletTx_;
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

Q_SIGNALS:
    void next(int pageID);
    void back(int pageID);
    void cancel(int pageID);

public Q_SLOTS:
    void onNext() { Q_EMIT next(pageID); }
    void onBack() { Q_EMIT back(pageID); }
    void onCancel() { Q_EMIT cancel(pageID); }

protected:
    WalletModel *walletModel;
    int pageID{0};
    BlocknetSendFundsModel *model = nullptr;

public:
    static QPair<QString, bool> processSendCoinsReturn(WalletModel *walletModel, const WalletModel::SendCoinsReturn & sendCoinsReturn, const QString & msgArg = QString()) {
        QPair<QString, bool> msgParams;
        // Default to a warning message, override if error message is needed
        msgParams.second = false;

        // This comment is specific to SendCoinsDialog usage of WalletModel::SendCoinsReturn.
        // WalletModel::TransactionCommitFailed is used only in WalletModel::sendCoins()
        // all others are used only in WalletModel::prepareTransaction()
        switch(sendCoinsReturn.status)
        {
            case WalletModel::InvalidAddress:
                msgParams.first = tr("The recipient address is not valid. Please recheck.");
                break;
            case WalletModel::InvalidAmount:
                msgParams.first = tr("The amount to pay must be larger than 0.");
                break;
            case WalletModel::AmountExceedsBalance:
                msgParams.first = tr("The amount exceeds your balance.");
                break;
            case WalletModel::AmountWithFeeExceedsBalance:
                msgParams.first = tr("The total exceeds your balance when the %1 transaction fee is included.").arg(msgArg);
                break;
            case WalletModel::DuplicateAddress:
                msgParams.first = tr("Duplicate address found: addresses should only be used once each.");
                break;
            case WalletModel::TransactionCreationFailed:
                msgParams.first = tr("The specified transaction information is bad, please review");
                msgParams.second = true;
                break;
            case WalletModel::TransactionCommitFailed:
                msgParams.first = tr("The transaction was rejected with the following reason: %1").arg(sendCoinsReturn.reasonCommitFailed);
                msgParams.second = true;
                break;
            case WalletModel::AbsurdFee:
                msgParams.first = tr("A fee higher than %1 is considered an absurdly high fee.")
                        .arg(BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), walletModel->node().getMaxTxFee()));
                break;
            case WalletModel::PaymentRequestExpired:
                msgParams.first = tr("Payment request expired.");
                msgParams.second = true;
                break;
                // included to prevent a compiler warning.
            case WalletModel::OK:
            default:
                return { QString(), false };
        }

        return { msgParams.first, msgParams.second };
    }
};

/**
 * @brief Double validator, rejects separators.
 */
class BlocknetNumberValidator : public QDoubleValidator {
    Q_OBJECT
public:
    explicit BlocknetNumberValidator(double bottom, double top, int decimals, QObject *parent = nullptr)
                                    : QDoubleValidator(bottom, top, decimals, parent)
    {
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

// tuple<fee, after_fee_amount, change>
static std::tuple<CAmount, CAmount, CAmount> BlocknetEstimateFee(WalletModel *walletModel, CCoinControl coinControl,
        const bool subtractFee, const QList<SendCoinsRecipient> & recipients)
{
    // Ignore the total fee designation
    coinControl.m_total_fee.reset();

    CAmount nPayAmount = 0;
    bool fDust = false;
    CMutableTransaction txDummy;
    for (const auto & recipient : recipients) {
        nPayAmount += recipient.amount;
        if (recipient.amount > 0) {
            CTxOut txout(recipient.amount, static_cast<CScript>(std::vector<unsigned char>(24, 0)));
            txDummy.vout.push_back(txout);
            fDust |= IsDust(txout, walletModel->node().getDustRelayFee());
        }
    }

    CAmount nAmount             = 0;
    CAmount nPayFee             = 0;
    CAmount nAfterFee           = 0;
    CAmount nChange             = 0;
    unsigned int nBytes         = 0;
    unsigned int nBytesInputs   = 0;
    unsigned int nQuantity      = 0;
    bool fWitness               = false;

    std::vector<COutPoint> vCoinControl;
    coinControl.ListSelected(vCoinControl);

    size_t i = 0;
    for (const auto& out : walletModel->wallet().getCoins(vCoinControl)) {
        if (out.depth_in_main_chain < 0) continue;

        // unselect already spent, very unlikely scenario, this could happen
        // when selected are spent elsewhere, like rpc or another computer
        const COutPoint& outpt = vCoinControl[i++];
        if (out.is_spent)
        {
            coinControl.UnSelect(outpt);
            continue;
        }

        // Quantity
        nQuantity++;

        // Amount
        nAmount += out.txout.nValue;

        // Bytes
        CTxDestination address;
        int witnessversion = 0;
        std::vector<unsigned char> witnessprogram;
        if (out.txout.scriptPubKey.IsWitnessProgram(witnessversion, witnessprogram))
        {
            nBytesInputs += (32 + 4 + 1 + (107 / WITNESS_SCALE_FACTOR) + 4);
            fWitness = true;
        }
        else if(ExtractDestination(out.txout.scriptPubKey, address))
        {
            CPubKey pubkey;
            CKeyID *keyid = boost::get<CKeyID>(&address);
            if (keyid && walletModel->wallet().getPubKey(*keyid, pubkey))
            {
                nBytesInputs += (pubkey.IsCompressed() ? 148 : 180);
            }
            else
                nBytesInputs += 148; // in all error cases, simply assume 148 here
        }
        else nBytesInputs += 148;
    }

    // calculation
    if (nQuantity > 0)
    {
        // Bytes
        nBytes = nBytesInputs + ((recipients.size() > 0 ? recipients.size() + 1 : 2) * 34) + 10; // always assume +1 output for change here
        if (fWitness)
        {
            // there is some fudging in these numbers related to the actual virtual transaction size calculation that will keep this estimate from being exact.
            // usually, the result will be an overestimate within a couple of satoshis so that the confirmation dialog ends up displaying a slightly smaller fee.
            // also, the witness stack size value is a variable sized integer. usually, the number of stack items will be well under the single byte var int limit.
            nBytes += 2; // account for the serialized marker and flag bytes
            nBytes += nQuantity; // account for the witness byte that holds the number of stack items for each input.
        }

        // in the subtract fee from amount case, we can tell if zero change already and subtract the bytes, so that fee calculation afterwards is accurate
        if (subtractFee)
            if (nAmount - nPayAmount == 0)
                nBytes -= 34;

        // Fee
        nPayFee = walletModel->wallet().getMinimumFee(nBytes, coinControl, nullptr /* returned_target */, nullptr /* reason */);

        if (nPayAmount > 0)
        {
            nChange = nAmount - nPayAmount;
            if (!subtractFee)
                nChange -= nPayFee;

            // Never create dust outputs; if we would, just add the dust to the fee.
            if (nChange > 0 && nChange < MIN_CHANGE)
            {
                CTxOut txout(nChange, static_cast<CScript>(std::vector<unsigned char>(24, 0)));
                if (IsDust(txout, walletModel->node().getDustRelayFee()))
                {
                    nPayFee += nChange;
                    nChange = 0;
                    if (subtractFee)
                        nBytes -= 34; // we didn't detect lack of change above
                }
            }

            if (nChange == 0 && !subtractFee)
                nBytes -= 34;
        }

        // after fee
        nAfterFee = std::max<CAmount>(nAmount - nPayFee, 0);
    }

    return std::make_tuple(nPayFee, nAfterFee, nChange);
}

#endif //BLOCKNET_QT_BLOCKNETSENDFUNDSUTIL_H
