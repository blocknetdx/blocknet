#include "termsofuse.h"
#include "ui_termsofuse.h"

#include "guiconstants.h"
#include "walletmodel.h"
#include "init.h"

#include <QMessageBox>
#include <QPushButton>
#include <QKeyEvent>
#include <QSettings>

#include <iostream>
#include <fstream>  

TermsOfUse::TermsOfUse(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TermsOfUse)
{
    ui->setupUi(this);

    connect(ui->buttonAgree, SIGNAL(clicked()), this, SLOT(clickAgree()));
    connect(ui->buttonCancel, SIGNAL(clicked()), this, SLOT(clickCancel()));
}

TermsOfUse::~TermsOfUse()
{
    delete ui;
}

void TermsOfUse::clickAgree()
{
    boost::filesystem::path pathDebug = GetDataDir() / ".agreed_to_tou";

    //touch file
    std::ofstream outfile (pathDebug.string().c_str());
    outfile << "I Agree!" << std::endl;
    outfile.close();
    
    close();
}

void TermsOfUse::clickCancel()
{
    Shutdown();
    close();
}