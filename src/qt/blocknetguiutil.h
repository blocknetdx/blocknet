// Copyright (c) 2011-2019 The Bitcoin Core developers
// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_BLOCKNETGUIUTIL_H
#define BITCOIN_QT_BLOCKNETGUIUTIL_H

#include <QApplication>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QDesktopWidget>

class BGU : public QObject {
public:
    static qreal spr(const qreal p) {
        const qreal target{96.0};
#if defined(Q_OS_MAC)
        const qreal dpi{target};
#else
        const qreal dpi{static_cast<qreal>(QApplication::desktop()->logicalDpiX())};
#endif
        if (dpi <= target)
            return p;
        qreal scale = dpi / target;
        return scale * p;
    }
    static int spi(const int p) {
        return static_cast<int>(spr(static_cast<qreal>(p)));
    }
    static qreal dpr() {
#if defined(Q_OS_MAC)
        const qreal device_pr = dynamic_cast<QGuiApplication*>(QCoreApplication::instance())->devicePixelRatio();
        return device_pr;
#else
        return spr(1.0);
#endif
    }
};

#endif // BITCOIN_QT_BLOCKNETGUIUTIL_H
