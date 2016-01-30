#ifndef DARKSENDCONFIG_H
#define DARKSENDCONFIG_H

#include <QDialog>

namespace Ui {
    class ObfuscateConfig;
}
class WalletModel;

/** Multifunctional dialog to ask for passphrases. Used for encryption, unlocking, and changing the passphrase.
 */
class ObfuscateConfig : public QDialog
{
    Q_OBJECT

public:

    ObfuscateConfig(QWidget *parent = 0);
    ~ObfuscateConfig();

    void setModel(WalletModel *model);


private:
    Ui::ObfuscateConfig *ui;
    WalletModel *model;
    void configure(bool enabled, int coins, int rounds);

private slots:

    void clickBasic();
    void clickHigh();
    void clickMax();
};

#endif // DARKSENDCONFIG_H
