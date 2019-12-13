// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETSETTINGS_H
#define BLOCKNET_QT_BLOCKNETSETTINGS_H

#include <qt/blocknetdropdown.h>
#include <qt/blocknetformbtn.h>
#include <qt/blocknetlabelbtn.h>
#include <qt/blocknetlineeditwithtitle.h>

#include <qt/walletmodel.h>

#include <QCheckBox>
#include <QDataWidgetMapper>
#include <QFrame>
#include <QLabel>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

class BlocknetSettings : public QFrame 
{
    Q_OBJECT

public:
    explicit BlocknetSettings(interfaces::Node & node, QWidget *parent = nullptr);
    void setWalletModel(WalletModel *w);
    void backupWallet();

private Q_SLOTS:
    void onResetSettingsToDefault();

private:
    interfaces::Node &node;
    QVBoxLayout *layout;
    QDataWidgetMapper *mapper;
    QScrollArea *scrollArea;
    QFrame *content;
    WalletModel *walletModel;
    QVBoxLayout *contentLayout;
    QLabel *titleLbl;
    BlocknetLabelBtn *aboutCoreLblBtn;
    BlocknetLabelBtn *aboutQtLblBtn;
    QLabel *generalLbl;
    QCheckBox *startWalletOnLoginCb;
    QLabel *sizeDbCacheLbl;
    QSpinBox *dbCacheSb;
    QLabel *verificationThreadsLbl;
    QSpinBox *threadsSb;
    QLabel *walletLbl;
    QCheckBox *spendChangeCb;
    BlocknetFormBtn *backupBtn;
    QLabel *networkLbl;
    QCheckBox *upnpCb;
    QCheckBox *allowIncomingCb;
    QCheckBox *connectSocks5Cb;
    BlocknetLineEditWithTitle *proxyTi;
    BlocknetLineEditWithTitle *portTi;
    QLabel *displayLbl;
    QLabel *languageLbl;
    BlocknetDropdown *languageDropdown;
    QLabel *contributeLbl;
    BlocknetLabelBtn *contributeLblBtn;
    QLabel *unitsLbl;
    BlocknetDropdown *unitsDropdown;
    QLabel *decimalLbl;
    BlocknetDropdown *decimalDropdown;
    BlocknetLineEditWithTitle *thirdPartyUrlTi;
    BlocknetFormBtn *saveBtn;
    BlocknetFormBtn *resetBtn;
};

#endif // BLOCKNET_QT_BLOCKNETSETTINGS_H
