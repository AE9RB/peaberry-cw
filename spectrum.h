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

#ifndef SPECTRUM_H
#define SPECTRUM_H

#include <QtCore>
#include "dsp.h"

class Spectrum : public QObject
{
    Q_OBJECT
public:
    explicit Spectrum(class Radio *radio);

signals:
    void spectrumViewUpdate(QVector<qreal>*);
    void smeterUpdate(qreal);
    void dcBiasUpdate(qreal real, qreal imag);
    void iqBalUpdate(qreal phase, qreal gain);

public slots:
    void setIir(int v);
    void setFilter(int hz);
    void setWindow(int w);
    void setDbOffset(qreal db);
    void spectrumUpdate(COMPLEX *raw, COMPLEX *adjusted, quint16 pos);

private:
    qreal iir;
    int m_filter;
    int m_window;
    qreal m_dbOffset;
    qreal sincSum;
    qreal basicSum;

    QVector<REAL> sincWin;
    QVector<REAL> basicWin;
    QVector<COMPLEX> fftBuf;
    QVector<REAL> iirBuf;
    QVector<qreal> fftAbs;

    int iqBalState = -1;
    unsigned int iqVerifyCount = 0;
    qreal iqPhase = 0;
    qreal iqGain = 1;
    qreal iqPhaseInTest;
    qreal iqGainInTest;
    qreal iqNewPhase;
    qreal iqNewGain;
    qreal iqReferenceDelta;
    QVector<unsigned int> iqSignalFinder;
    QVector<COMPLEX> iqRawData;
    QVector<COMPLEX> iqDataInTest;

};

#endif // SPECTRUM_H
