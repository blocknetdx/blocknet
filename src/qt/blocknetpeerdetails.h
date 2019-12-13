// Copyright (c) 2018-2019 The Blocknet developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BLOCKNET_QT_BLOCKNETPEERDETAILS_H
#define BLOCKNET_QT_BLOCKNETPEERDETAILS_H

#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

class BlocknetPeerDetails : public QFrame {
    Q_OBJECT
public:
    explicit BlocknetPeerDetails(QFrame *parent = nullptr);
    ~BlocknetPeerDetails() override;

    struct PeerDetails {
        QString direction;
        QString protocol;
        QString version;
        QString startingHeight;
        QString syncHeight;
        QString banScore;
        QString connectionTime;
        QString lastSend;
        QString lastReceive;
        QString bytesSent;
        QString bytesReceived;
        QString pingTime;
    };

    void setDisplayWidget(QWidget *widget);
    void show(PeerDetails details);

public Q_SLOTS:
    void removeSelf(bool kill = true);

private:
    QVBoxLayout *layout;
    QGridLayout *detailsLayout;
    QFrame *detailsFrame;
    QWidget *displayWidget = nullptr;
    QLabel *titleLbl;
    QLabel *directionLbl;
    QLabel *protocolLbl;
    QLabel *versionLbl;
    QLabel *startHeightLbl;
    QLabel *syncHeightLbl;
    QLabel *banScoreLbl;
    QLabel *connTimeLbl;
    QLabel *lastSendLbl;
    QLabel *lastReceiveLbl;
    QLabel *bytesSentLbl;
    QLabel *bytesReceivedLbl;
    QLabel *pingTimeLbl;

    bool eventFilter(QObject *obj, QEvent *event) override;
};

#endif // BLOCKNET_QT_BLOCKNETPEERDETAILS_H