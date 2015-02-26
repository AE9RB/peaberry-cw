// Peaberry CW - Transceiver for Peaberry SDR
// Copyright (C) 2015 David Turnbull AE9RB
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef CAT_H
#define CAT_H

#include <QtCore>

class Cat : public QObject {
    Q_OBJECT
public:
    Cat(class Radio *radio);
    ~Cat();
    class CATworker *worker;
    QString error;
    QString serialNumber;
private:
    int findPeaberryDevice();
    QString readUsbString(struct usb_dev_handle *udh, quint8 iDesc);
    struct usb_device *dev;
    struct usb_dev_handle *udh;
    class QTimer *timer;
    class QElapsedTimer *etimer;
    class Keyer *keyer;
    qint64 transmitMark;
    qint64 freqChangeMark;
    qint64 currentFreq;
    qint64 requestedFreq;
    qint64 freq;
    qint64 xit;
    qint64 rit;
    bool transmitOK;
    int transmitPower;
    qreal paddingSecs;
signals:
    void sendElement(qreal keySecs, qreal txSecs);
public slots:
    void start();
    void stop();
    void setFreq(qint64 f);
    void setXit(qint64 f);
    void setRit(qint64 f);
    void setPower(int p);
    void setTransmitPadding(qreal secs);
private slots:
    void doWork();
    void approveTransmit();
};

#endif // CAT_H
