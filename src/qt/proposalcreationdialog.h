#ifndef PROPOSALCREATIONDIALOG_H
#define PROPOSALCREATIONDIALOG_H

#include <QDialog>
#include <QAbstractButton>

namespace Ui {
class ProposalCreationDialog;
}

class ProposalCreationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProposalCreationDialog(QWidget *parent = 0, bool enableWallet = false);
    ~ProposalCreationDialog();

private slots:
    void on_closebuttonBox_clicked(QAbstractButton *button);

    void on_nametextEdit_textChanged();

    void on_preparepushButton_clicked();

    void on_urltextEdit_textChanged();

    void on_blockstarttextEdit_textChanged();

    void on_paymentcounttextEdit_textChanged();

    void on_monthlyblockdxtextEdit_textChanged();

    void on_blocknetdxaddresstextEdit_textChanged();

    void on_feetxtextEdit_textChanged();

    void on_submitpushButton_clicked();    

    void on_ProposalCreationDialog_rejected();

private:
    Ui::ProposalCreationDialog *ui;

    typedef struct preparePropDataTypeTag {
        QString Name;
        QString URL;
        int64_t TotalPaymentCount;
        int64_t BlockStart;
        QString BlocknetDxAddress;
        double MonthlyPayment;

    } preparePropDataType;

    preparePropDataType preparePropData;
    QString submitFeeHash;
    int prepareWaitClk;
    int timerId;
    QTime* n;

protected:
    void timerEvent(QTimerEvent *event);


};

#endif // PROPOSALCREATIONDIALOG_H
