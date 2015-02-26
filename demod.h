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

#ifndef DEMOD_H
#define DEMOD_H

#include <QtCore>
#include "dsp.h"

class Demod : public QObject
{
    Q_OBJECT
public:
    explicit Demod(class Radio *radio, class Audio *audio);
    ~Demod();

private:
    class Radio *radio;
    class Audio *audio;
    class Agc *agc;

    bool cwr;
    qreal gain;
    int filterWidth;
    int tone;

    // mixAndDecimate state
    std::complex<qreal> madOsc;
    std::complex<qreal> madInc;
    COMPLEX madBuf[22];
    int madPos;

    // firOvSv state
    std::complex<qreal> ovsvOsc;
    std::complex<qreal> ovsvInc;
    QVector<COMPLEX> ovsvFilter;
    QVector<COMPLEX> ovsvWork;

    // resampler state
    static const int RESAMPLE_POSITIONS = 4000;
    static const int RESAMPLE_SINC_SIZE = 16; // keep even
    qreal resamplePos;
    qreal resampleRate;
    QVector<REAL> resampleTable;

    // temporary work space
    QVector<COMPLEX> tempData;

signals:

public slots:
    void setGain(int v);
    void setFilter(int hz);
    void setTone(int hz);
    void setCwr(bool r);
    void demod(COMPLEX *data);

private:
    void mixAndDecimate(COMPLEX *inData, COMPLEX *outData);
    void configureFirOvSvMixer();
    void configureFirOvSvFilter();
    void firOvSv(COMPLEX *inData, COMPLEX *outData);
    void setupResampler();
    void resample(COMPLEX *inData);

};

#endif // DEMOD_H
