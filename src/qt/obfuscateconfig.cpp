#include "obfuscateconfig.h"
#include "ui_obfuscateconfig.h"

#include "bitcoinunits.h"
#include "guiconstants.h"
#include "optionsmodel.h"
#include "walletmodel.h"
#include "init.h"

#include <QMessageBox>
#include <QPushButton>
#include <QKeyEvent>
#include <QSettings>

ObfuscateConfig::ObfuscateConfig(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ObfuscateConfig),
    model(0)
{
    ui->setupUi(this);

    connect(ui->buttonBasic, SIGNAL(clicked()), this, SLOT(clickBasic()));
    connect(ui->buttonHigh, SIGNAL(clicked()), this, SLOT(clickHigh()));
    connect(ui->buttonMax, SIGNAL(clicked()), this, SLOT(clickMax()));
}

ObfuscateConfig::~ObfuscateConfig()
{
    delete ui;
}

void ObfuscateConfig::setModel(WalletModel *model)
{
    this->model = model;
}

void ObfuscateConfig::clickBasic()
{
    configure(true, 1000, 2);

    QString strAmount(BitcoinUnits::formatWithUnit(
        model->getOptionsModel()->getDisplayUnit(), 10000 * COIN));
    QMessageBox::information(this, tr("Obfuscation Configuration"),
        tr(
            "Obfuscation was successfully set to basic (%1 and 2 rounds). You can change this at any time by opening DarkNet's configuration screen."
        ).arg(strAmount)
    );

    close();
}

void ObfuscateConfig::clickHigh()
{
    configure(true, 1000, 8);

    QString strAmount(BitcoinUnits::formatWithUnit(
        model->getOptionsModel()->getDisplayUnit(), 10000 * COIN));
    QMessageBox::information(this, tr("Obfuscation Configuration"),
        tr(
            "Obfuscation was successfully set to high (%1 and 8 rounds). You can change this at any time by opening DarkNet's configuration screen."
        ).arg(strAmount)
    );

    close();
}

void ObfuscateConfig::clickMax()
{
    configure(true, 1000, 16);

    QString strAmount(BitcoinUnits::formatWithUnit(
        model->getOptionsModel()->getDisplayUnit(), 10000 * COIN));
    QMessageBox::information(this, tr("Obfuscation Configuration"),
        tr(
            "Obfuscation was successfully set to maximum (%1 and 16 rounds). You can change this at any time by opening DarkNet's configuration screen."
        ).arg(strAmount)
    );

    close();
}

void ObfuscateConfig::configure(bool enabled, int coins, int rounds) {

    QSettings settings;

    settings.setValue("nObfuscateRounds", rounds);
    settings.setValue("nAnonymizeDarknetAmount", coins);

    nObfuscateRounds = rounds;
    nAnonymizeDarknetAmount = coins;
}
