// Copyright (c) 2011-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "privacydialog.h"
#include "ui_privacydialog.h"

#include "addressbookpage.h"
#include "addresstablemodel.h"
#include "bitcoinunits.h"
#include "guiutil.h"
#include "obfuscation.h"
#include "obfuscationconfig.h"
#include "optionsmodel.h"
#include "receiverequestdialog.h"
#include "recentrequeststablemodel.h"
#include "walletmodel.h"

#include <QAction>
#include <QCursor>
#include <QItemSelection>
#include <QMessageBox>
#include <QScrollBar>
#include <QSettings>
#include <QTextDocument>


#include <QPieSeries>
#include <QPieSlice>
#include <QChart>
#include <QChartView>

PrivacyDialog::PrivacyDialog(QWidget* parent) : QDialog(parent),
                                                          ui(new Ui::PrivacyDialog),
                                                          walletModel(0),
                                                          currentBalance(-1)
{
    nDisplayUnit = 0; // just make sure it's not unitialized
    ui->setupUi(this);
    ui->labelzPIVSyncStatus->setText("(" + tr("out of sync") + ")");

    QPieSeries *series = new QPieSeries();
    series->append("Jane", 1);
    series->append("Joe", 2);
    series->append("Andy", 3);
    series->append("Barbara", 4);
    series->append("Axel", 5);

    QPieSlice *slice = series->slices().at(1);
    slice->setExploded();
    slice->setLabelVisible();
    slice->setPen(QPen(Qt::darkGreen, 2));
    slice->setBrush(Qt::green);

    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("Simple piechart example");
    chart->legend()->hide();

    QChartView *chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    
//    if (fMasterNode) {
//        ui->pushButtonStartMixing->setText("(" + tr("Disabled") + ")");
//        ui->pushButtonRetryMixing->setText("(" + tr("Disabled") + ")");
//        ui->pushButtonResetMixing->setText("(" + tr("Disabled") + ")");
//    } else {
//        if (!fEnableObfuscation) {
//            ui->pushButtonStartMixing->setText(tr("Start"));
//        } else {
//            ui->pushButtonStartMixing->setText(tr("Stop"));
//        }
//        timer = new QTimer(this);
//        connect(timer, SIGNAL(timeout()), this, SLOT(obfuScationStatus()));
//        timer->start(1000);
//    }
    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);
}

PrivacyDialog::~PrivacyDialog()
{
    delete ui;
}

void PrivacyDialog::setModel(WalletModel* walletModel)
{
    this->walletModel = walletModel;

    if (walletModel && walletModel->getOptionsModel()) {
        // Keep up to date with wallet
//        setBalance(walletModel->getBalance(), walletModel->getAnonymizedBalance());
        connect(walletModel, SIGNAL(balanceChanged(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)), this, SLOT(setBalance(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)));
//        connect(ui->pushButtonRetryMixing, SIGNAL(clicked()), this, SLOT(obfuscationAuto()));
//        connect(ui->pushButtonResetMixing, SIGNAL(clicked()), this, SLOT(obfuscationReset()));
//        connect(ui->pushButtonStartMixing, SIGNAL(clicked()), this, SLOT(toggleObfuscation()));
    }
}

void PrivacyDialog::setBalance(const CAmount& balance, const CAmount& anonymizedBalance)
{
    currentBalance = balance;
    currentAnonymizedBalance = anonymizedBalance;
//    updateObfuscationProgress();
}

void PrivacyDialog::updateDisplayUnit()
{
    if (walletModel && walletModel->getOptionsModel()) {
        nDisplayUnit = walletModel->getOptionsModel()->getDisplayUnit();
        if (currentBalance != -1)
            setBalance(currentBalance, currentAnonymizedBalance);

//        // Update txdelegate->unit with the current unit
//        txdelegate->unit = nDisplayUnit;
//
//        ui->listTransactions->update();
    }
}

void PrivacyDialog::showOutOfSyncWarning(bool fShow)
{
    ui->labelzPIVSyncStatus->setVisible(fShow);
}
