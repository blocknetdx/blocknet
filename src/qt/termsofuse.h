#ifndef TERMSOFUSE_H
#define TERMSOFUSE_H

#include <QDialog>

namespace Ui {
    class TermsOfUse;
}
/** Multifunctional dialog to ask for passphrases. Used for encryption, unlocking, and changing the passphrase.
 */
class TermsOfUse : public QDialog
{
    Q_OBJECT

public:
    TermsOfUse(QWidget *parent = 0);
    ~TermsOfUse();

private:
    Ui::TermsOfUse *ui;
    void configure(bool enabled, int coins, int rounds);

private slots:

    void clickAgree();
    void clickCancel();
};

#endif // TERMSOFUSE_H
