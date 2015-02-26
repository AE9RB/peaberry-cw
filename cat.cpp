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

#include "cat.h"
#include "usb.h"
#include "radio.h"
#include "keyer.h"
#include <QTimer>
#include <QElapsedTimer>

Cat::Cat(Radio *radio) :
    keyer(nullptr)
{
    paddingSecs = 0;
    transmitMark = 0;
    freqChangeMark = 0;
    xit = rit = 0;
    freq = currentFreq = 0;
    setFreq(33333333); // Default I/Q ordering

    int qtyfound = findPeaberryDevice();

    if (qtyfound == 0) {
        error = "No radio hardware found. "
                #ifdef Q_OS_WIN
                "Make sure the Peaberry SDR is connected, powered, "
                "and you have installed the libusb-win32 "
                "device driver.";
                #else
                "Make sure the Peaberry SDR is connected and powered.";
                #endif
        return;
    }

    if (qtyfound > 1) {
        error = "Found more than one Peaberry SDR. "
                "Unplug all radios except the one you want to use.";
        return;
    }

    connect(radio, SIGNAL(freqChanged(qint64)), this, SLOT(setFreq(qint64)));
    connect(radio, SIGNAL(xitChanged(qint64)), this, SLOT(setXit(qint64)));
    connect(radio, SIGNAL(ritChanged(qint64)), this, SLOT(setRit(qint64)));
    connect(radio, SIGNAL(powerChanged(int)), this, SLOT(setPower(int)));

    keyer = new Keyer(radio, this);
}

Cat::~Cat()
{
    delete keyer;
}

int Cat::findPeaberryDevice() {

    int qtyfound = 0;
    struct usb_bus *bus;
    struct usb_device *curDev;
    struct usb_dev_handle *udh;
    QString name;

    usb_init();
    usb_find_busses();
    usb_find_devices();

    for (bus = usb_get_busses(); bus != NULL; bus = bus->next) {
        for (curDev = bus->devices; curDev != NULL; curDev = curDev->next) {

            if (curDev->descriptor.iProduct != 0 &&
                    curDev->descriptor.idVendor == 0x16C0 &&
                    curDev->descriptor.idProduct == 0x05DC &&
                    (udh = usb_open(curDev))) {

                name = readUsbString(udh, curDev->descriptor.iProduct);

                if (name == QStringLiteral("DG8SAQ-I2C") ||
                        name == QStringLiteral("Peaberry SDR")) {

                    serialNumber = readUsbString(udh, curDev->descriptor.iSerialNumber);
                    qtyfound++;
                    dev = curDev;
                }

                usb_close(udh);
            }
        }
    }

    return qtyfound;
}

QString Cat::readUsbString(struct usb_dev_handle *udh, quint8 iDesc) {
    char buffer[256];
    int	rval;
    QString s;
    if (iDesc) {
        rval = usb_control_msg(udh, USB_ENDPOINT_IN, USB_REQ_GET_DESCRIPTOR,
                               (USB_DT_STRING << 8) + iDesc, 0x0409,
                               buffer, sizeof(buffer), 100);
        if (rval >= 0 && buffer[1] == USB_DT_STRING) {
            if((unsigned char)buffer[0] < rval) {
                rval = (unsigned char)buffer[0];
            }
            s.setUtf16((ushort*)buffer+1, rval/2-1);
        }
    }
    return s;
}

void Cat::start()
{
    udh = usb_open(dev);
    #ifdef Q_OS_WIN
    // The libusb-win32 docs say this is necessary.
    // On OSX it will cause the audio device to momentatily vanish.
    usb_set_configuration(udh, dev->config->bConfigurationValue);
    #endif
    timer = new QTimer(this);
    timer->setInterval(0);
    connect(timer, SIGNAL(timeout()), this, SLOT(doWork()));
    timer->start();
    etimer = new QElapsedTimer();
    etimer->start();
}

void Cat::stop()
{
    // Ensure we stop transmitting
    usb_control_msg(udh, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN,
                    0x50, 0, 0, 0, 0, 10);
    delete timer;
    delete etimer;
    usb_close(udh);
}

void Cat::setFreq(qint64 f)
{
    freq = f;
    requestedFreq = f + rit;
    approveTransmit();
}

void Cat::setXit(qint64 f)
{
    xit = f;
    approveTransmit();
}

void Cat::setRit(qint64 f)
{
    rit = f;
    requestedFreq = f + freq;
}

void Cat::setPower(int p)
{
    transmitPower = p;
}

void Cat::setTransmitPadding(qreal secs)
{
    paddingSecs = secs;
}

void Cat::doWork()
{
    const int TIMEOUT = 3;
    union {
        quint32 freq;
        quint8 key;
    } buffer;
    int ret, request;
    qint64 mark;

    mark = etimer->nsecsElapsed();

    if (mark < transmitMark) request = 1;
    else request = 0;

    ret = usb_control_msg(udh, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN,
                          0x50, request, 0, (char*)&buffer, sizeof(buffer), TIMEOUT);

    if (ret != 1) {
        static bool waswarned = false;
        if (!waswarned) {
            waswarned = true;
            qWarning() << "cat TIMEOUT too short";
        }
        // Sleep keeps from consuming all CPU when radio unplugged.
        QThread::msleep(1);
    } else {
        keyer->keyUpdate(!(buffer.key&0x20),!(buffer.key&0x02));
    }

    qreal sendTime = keyer->keyProcess(mark);
    if (sendTime >= 0) {
        if (transmitOK && transmitPower) {
            emit sendElement(sendTime, sendTime);
            // Fresh mark to remove USB delay
            mark = etimer->nsecsElapsed();
            transmitMark = mark +
                           (sendTime + paddingSecs) * 1000000000;
        } else {
            emit sendElement(sendTime, -1);
        }
    }

    if (currentFreq != requestedFreq && mark > freqChangeMark) {
        buffer.freq = (double)(requestedFreq+24000)/1000000 * (1UL << 23);
        ret = usb_control_msg(udh, USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT,
                              0x32, 0, 0, (char*)&buffer, sizeof(buffer), TIMEOUT);
        if (ret == 4) {
            freqChangeMark = mark + 10000000;
            currentFreq = requestedFreq;
        }
    }

}

void Cat::approveTransmit()
{
    // Disable transmit outside ham bands
    // Note 30m is 10.1-10.1573 for ITU 3
    qint64 f = freq + xit;
    transmitOK = false;
    if (f >= 1800000 && f <= 2000000) transmitOK = true;
    if (f >= 3500000 && f <= 4000000) transmitOK = true;
    if (f >= 5000000 && f <= 5500000) transmitOK = true;
    if (f >= 7000000 && f <= 7300000) transmitOK = true;
    if (f >= 10100000 && f <= 10157300) transmitOK = true;
    if (f >= 14000000 && f <= 14350000) transmitOK = true;
    if (f >= 18068000 && f <= 18168000) transmitOK = true;
    if (f >= 21000000 && f <= 21450000) transmitOK = true;
    if (f >= 24890000 && f <= 24990000) transmitOK = true;
    if (f >= 28000000 && f <= 29700000) transmitOK = true;
}
