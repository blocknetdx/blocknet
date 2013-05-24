#include "splashscreen.h"
#include "clientversion.h"
#include "util.h"

#include <QPainter>
#include <QApplication>

SplashScreen::SplashScreen(const QPixmap &pixmap, Qt::WindowFlags f) :
    QSplashScreen(pixmap, f)
{
    // set reference point, paddings
    int paddingLeft             = 40;
    int paddingTop              = 386;
    int titleVersionVSpace      = 17;
    int marginCopyrightLeft     = 220;
    int titleCopyrightVSpace1    = -1;
    int titleCopyrightVSpace2    = 16;

    float fontFactor            = 1.0;

    // define text to place
    QString titleText       = QString(QApplication::applicationName()).replace(QString("-testnet"), QString(""), Qt::CaseSensitive); // cut of testnet, place it as single object further down
    QString versionText     = QString("Version %1").arg(QString::fromStdString(FormatFullVersion()));
    QString copyrightText1   = QChar(0xA9)+QString(" 2009-%1 ").arg(COPYRIGHT_YEAR) + QString(tr("The Bitcoin developers"));
    QString copyrightText2   = QChar(0xA9)+QString(" 20011-%1 ").arg(COPYRIGHT_YEAR) + QString(tr("The Litecoin developers"));
    QString testnetAddText  = QString(tr("[testnet]")); // define text to place as single text object

    QString font            = "Arial";

    // load the bitmap for writing some text over it
    QPixmap newPixmap;
    if(GetBoolArg("-testnet")) {
        newPixmap     = QPixmap(":/images/splash_testnet");
    }
    else {
        newPixmap     = QPixmap(":/images/splash");
    }

    QPainter pixPaint(&newPixmap);
    pixPaint.setPen(QColor(60,60,60));

    // check font size and drawing with
    pixPaint.setFont(QFont(font, fontFactor));
    QFontMetrics fm = pixPaint.fontMetrics();
    int titleTextWidth  = fm.width(titleText);
    if(titleTextWidth > 160) {
        // strange font rendering, Arial probably not found
        fontFactor = 0.75;
    }

    pixPaint.setFont(QFont(font, 20*fontFactor));
    fm = pixPaint.fontMetrics();
    titleTextWidth  = fm.width(titleText);
    pixPaint.drawText(paddingLeft,paddingTop,titleText);

    pixPaint.setFont(QFont(font, 10*fontFactor));

    // if the version string is to long, reduce size
    fm = pixPaint.fontMetrics();
    int versionTextWidth  = fm.width(versionText);

    pixPaint.drawText(paddingLeft,paddingTop+titleVersionVSpace,versionText);

    // draw copyright stuff
    pixPaint.setFont(QFont(font, 10*fontFactor));
    pixPaint.drawText(marginCopyrightLeft,paddingTop+titleCopyrightVSpace1,copyrightText1);
    pixPaint.drawText(marginCopyrightLeft,paddingTop+titleCopyrightVSpace2,copyrightText2);

    // draw testnet string if -testnet is on
    if(QApplication::applicationName().contains(QString("-testnet"))) {
        // draw copyright stuff
        QFont boldFont = QFont(font, 10*fontFactor);
        boldFont.setWeight(QFont::Bold);
        pixPaint.setFont(boldFont);
        fm = pixPaint.fontMetrics();
        int testnetAddTextWidth  = fm.width(testnetAddText);
        pixPaint.drawText((newPixmap.width()-testnetAddTextWidth)/2,13,testnetAddText);
    }

    pixPaint.end();

    this->setPixmap(newPixmap);
}
