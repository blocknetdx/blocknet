#include "proposalcreationdialog.h"
#include "ui_proposalcreationdialog.h"
#include "../rpcserver.h"
#include "../init.h"
#include "../wallet.h"
#include "../qt/bitcoingui.h"
#include"../qt/overviewpage.h"
#include <QTimer>
#include <QTime>

using namespace json_spirit;
using namespace std;


ProposalCreationDialog::ProposalCreationDialog(QWidget *parent, bool enableWallet) : QDialog(parent), ui(new Ui::ProposalCreationDialog)
{
    ui->setupUi(this);
    Qt::WindowFlags flags = Qt::Window | Qt::WindowSystemMenuHint
                                | Qt::WindowMinimizeButtonHint
                                | Qt::WindowCloseButtonHint;
    setWindowFlags(flags);

    ui->submitpushButton->setEnabled(false);
    ui->submitpushButton->setStyleSheet(QString::fromUtf8("QPushButton:disabled { color: gray }"));
    ui->preparepushButton->setStyleSheet(QString::fromUtf8("QPushButton:disabled { color: gray }"));
    ui->nametextEdit->setText("proposal-1");
    ui->urltextEdit->setText("https://www.yoururlhere.com");
    ui->monthlyblockdxtextEdit->setText("10");
    ui->paymentcounttextEdit->setText("2");

    Array params;
    params.push_back("");

    json_spirit::Value retVal = getaccountaddress(params, false);
    QString result = QString::fromStdString(json_spirit::write_string(retVal));
    result.remove(0, 1);
    result.remove(result.length()-1, 1);
    ui->blocknetdxaddresstextEdit->setText(result);

    params.clear();
    params.push_back("nextblock");
    retVal = mnbudget(params, false);
    result = QString::fromStdString(json_spirit::write_string(retVal));
    ui->blockstarttextEdit->setText(result);

    prepareWaitClk = 0;
    n = new QTime();
}


ProposalCreationDialog::~ProposalCreationDialog()
{
    delete ui;
    delete n;
}


void ProposalCreationDialog::on_closebuttonBox_clicked(QAbstractButton *button)
{
    BitcoinGUI* mainWnd = (BitcoinGUI*)(this->parentWidget());
    mainWnd->proposalCreationActive = false;
    QDialog::close();
}


void ProposalCreationDialog::on_nametextEdit_textChanged()
{
    preparePropData.Name = ui->nametextEdit->toPlainText();
}


void ProposalCreationDialog::on_urltextEdit_textChanged()
{
    preparePropData.URL = ui->urltextEdit->toPlainText();
}


void ProposalCreationDialog::on_blockstarttextEdit_textChanged()
{
    preparePropData.BlockStart = ui->blockstarttextEdit->toPlainText().toInt();
}


void ProposalCreationDialog::on_paymentcounttextEdit_textChanged()
{
    preparePropData.TotalPaymentCount = ui->paymentcounttextEdit->toPlainText().toInt();
}


void ProposalCreationDialog::on_monthlyblockdxtextEdit_textChanged()
{
    preparePropData.MonthlyPayment = ui->monthlyblockdxtextEdit->toPlainText().toDouble();
}


void ProposalCreationDialog::on_blocknetdxaddresstextEdit_textChanged()
{
    preparePropData.BlocknetDxAddress = ui->blocknetdxaddresstextEdit->toPlainText();
}


void ProposalCreationDialog::on_feetxtextEdit_textChanged()
{
    submitFeeHash = ui->feetxtextEdit->toPlainText();
}


void ProposalCreationDialog::on_preparepushButton_clicked()
{
    Array prepareParams;

    ui->submitpushButton->setEnabled(false);

    prepareParams.push_back("prepare");
    prepareParams.push_back(preparePropData.Name.toStdString().c_str());
    prepareParams.push_back(preparePropData.URL.toStdString().c_str());
    prepareParams.push_back(preparePropData.TotalPaymentCount);
    prepareParams.push_back(preparePropData.BlockStart);
    prepareParams.push_back(preparePropData.BlocknetDxAddress.toStdString().c_str());
    prepareParams.push_back(preparePropData.MonthlyPayment);

    json_spirit::Value retVal = mnbudget(prepareParams, false);
    QString result = QString::fromStdString(json_spirit::write_string(retVal));
    result.remove(0, 1);
    result.remove(result.length()-1, 1);
    ui->statuslabel->setText("Current Status: " + result);
    if (result.length() == 64)
    {
        ui->preparepushButton->setEnabled(false);
        ui->feetxtextEdit->clear();
        ui->feetxtextEdit->setText(result);

        timerId = startTimer(1000);
        n->setHMS(0, 8, 0);
        ui->statuslabel->setText("Current Status: Proposal Submission ready in - " + n->toString());
    }
}



void ProposalCreationDialog::on_submitpushButton_clicked()
{
    Array submitParams;

    submitParams.push_back("submit");
    submitParams.push_back(preparePropData.Name.toStdString().c_str());
    submitParams.push_back(preparePropData.URL.toStdString().c_str());
    submitParams.push_back(preparePropData.TotalPaymentCount);
    submitParams.push_back(preparePropData.BlockStart);
    submitParams.push_back(preparePropData.BlocknetDxAddress.toStdString().c_str());
    submitParams.push_back(preparePropData.MonthlyPayment);
    submitParams.push_back(submitFeeHash.toStdString().c_str());

    json_spirit::Value retVal = mnbudget(submitParams, false);
    ui->statuslabel->setText("Current Status: " + QString::fromStdString(json_spirit::write_string(retVal)));
    ui->preparepushButton->setEnabled(true);
    ui->submitpushButton->setEnabled(false);
}


void ProposalCreationDialog::timerEvent(QTimerEvent *event)
{
    if (n->minute() == 0 && n->second() == 0)
    {
       ui->submitpushButton->setEnabled(true);
       ui->statuslabel->setText("Current Status: Proposal Submission READY");
    }
    else
    {
        *n = n->addSecs(-1);
        ui->statuslabel->setText("Current Status: Proposal Submission ready in - " + n->toString());
    }
}


void ProposalCreationDialog::on_ProposalCreationDialog_rejected()
{
    BitcoinGUI* mainWnd = (BitcoinGUI*)(this->parentWidget());
    mainWnd->proposalCreationActive = false;
}
