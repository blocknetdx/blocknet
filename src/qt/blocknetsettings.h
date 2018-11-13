// Copyright (c) 2018 The Blocknet Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNETSETTINGS_H
#define BLOCKNETSETTINGS_H

#include "walletmodel.h"
#include "blocknetdropdown.h"
#include "blocknetformbtn.h"
#include "blocknetlineeditwithtitle.h"
#include "blocknetlabelbtn.h"

#include <QScrollArea>
#include <QFrame>
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QCheckBox>
#include <QDataWidgetMapper>

class BlocknetSettings : public QFrame 
{
    Q_OBJECT

public:
    explicit BlocknetSettings(QWidget *parent = nullptr);
    void setWalletModel(WalletModel *w);
    void backupWallet();

private slots:
    void onResetSettingsToDefault();

private:
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
    BlocknetFormBtn *showBackupsBtn;
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

#endif // BLOCKNETSETTINGS_H
