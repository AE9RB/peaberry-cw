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

#include "spectrum.h"
#include "radio.h"
#include "fft.h"
#include "dsp.h"

Spectrum::Spectrum(Radio *radio) :
    sincWin(49152),
    basicWin(8192),
    fftBuf(8192),
    iirBuf(8192),
    fftAbs(2560),
    iqSignalFinder(8192),
    iqRawData(8192),
    iqDataInTest(8192)
{
    setIir(50);
    setFilter(500);
    setDbOffset(0);
    setWindow(0);

    // Compute sinc window for polyphase FFT
    qreal len, prd, n;
    len = sincWin.size() / 2;
    prd = sincWin.size() / 3;
    n = 0.5;
    sincSum = 0;
    for (auto &v : sincWin) {
        auto x = (2*M_PI*(n-len))/prd;
        v = sin(x) / x;
        sincSum += v;
        ++n;
    }

    connect(this, SIGNAL(spectrumViewUpdate(QVector<qreal>*)),
            radio, SIGNAL(spectrumViewUpdate(QVector<qreal>*)));

    connect(radio, SIGNAL(fftFilterChanged(int)), this, SLOT(setIir(int)));
    connect(radio, SIGNAL(filterChanged(int)), this, SLOT(setFilter(int)));
    connect(radio, SIGNAL(windowChanged(int)), this, SLOT(setWindow(int)));
    connect(radio, SIGNAL(dbOffsetChanged(qreal)), this, SLOT(setDbOffset(qreal)));

    connect(this, SIGNAL(iqBalUpdate(qreal,qreal)), radio, SIGNAL(rxIqBalUpdate(qreal,qreal)));
    connect(this, SIGNAL(dcBiasUpdate(qreal,qreal)), radio, SIGNAL(rxDcBiasUpdate(qreal,qreal)));
    connect(this, SIGNAL(smeterUpdate(qreal)), radio, SIGNAL(smeterUpdate(qreal)));
}

void Spectrum::setIir(int v)
{
    iir = -1.0 / ((v+1)/5.0);
}

void Spectrum::setFilter(int hz)
{
    // temporary for s-meter hack
    m_filter = hz + 75;
}

void Spectrum::setWindow(int w)
{
    qreal n, len;
    switch(m_window = w) {
    case 1: // hann
        basicSum = 0;
        len = basicWin.size() - 1;
        n = 0;
        for (auto &v : basicWin) {
            v = .5 - .5 * cos((2*M_PI*n)/len);
            basicSum += v;
            ++n;
        }
        break;
    case 2: // rectangular
        basicSum = 0;
        for (auto &v : basicWin) {
            v = 1.0;
            basicSum += v;
        }
        break;
    default: // 0 polyphase
        break;
    }
}

void Spectrum::setDbOffset(qreal db)
{
    m_dbOffset = db;
}

void Spectrum::spectrumUpdate(COMPLEX *raw, COMPLEX *adjusted, quint16 pos)
{
    unsigned int i;
    i = 0;
    qreal winSum;
    if (m_window) {
        winSum = basicSum;
        pos -= 8192;
        while (i < 8192) {
            fftBuf[i] = adjusted[pos] * basicWin[i];
            i++;
            pos++;
        }
    } else {
        winSum = sincSum;
        pos -= 49152;
        while (i < 8192) {
            fftBuf[i & 0x1FFF] = adjusted[pos] * sincWin[i];
            i++;
            pos++;
        }
        while (i < 49152) {
            fftBuf[i & 0x1FFF] += adjusted[pos] * sincWin[i];
            i++;
            pos++;
        }
    }

    FFT::dft(*(COMPLEX(*)[8192])fftBuf.data());
    for (i = 0; i < 2560; ++i) {
        auto v = fftBuf[i+4864];
        qreal gain = 1.0 - exp(iir * abs(exp(v)));
        iirBuf[i] = iirBuf[i] * (1-gain) + abs(v) / winSum * gain;
        fftAbs[i] = 20 * log10(iirBuf[i]) + m_dbOffset;
    }
    emit spectrumViewUpdate(&fftAbs);

    // S-meter is super-cheesy peak reading of FFT.
    // This will eventually be done with demod analysis.
    unsigned int smeterHalfWidth = (m_filter+75) / (96000.0 / 8192.0) / 2;
    qreal smeter = -999;
    for (i = 6143-smeterHalfWidth; i < 6144+smeterHalfWidth; i++) {
        qreal v = 20 * log10(abs(fftBuf[i]) / winSum);
        if (v > smeter) smeter = v;
    }
    emit smeterUpdate(smeter + m_dbOffset);

    // Optimistic bias adjustment.
    // Assumes future samples will be similar to past samples.
    COMPLEX dcbias;
    pos -= 49152;
    for (i=0; i<49152; i++) {
        dcbias += raw[pos];
        pos++;
    }
    dcbias /= 49152;
    emit dcBiasUpdate(dcbias.real(), dcbias.imag());

    // A signal bin must be 20dB over its mirror for this
    // many passes in a row before it's used for balancing.
    // Potential adjustments must also succeed this many
    // times in a row.
    const unsigned int sigCountThreshold = 5;

    if (iqBalState == -1) {
        // Hunting for signals we can balance
        for (i = 0; i < 2560; ++i) {
            qreal x1 = std::abs(fftBuf[4864+i])/winSum;
            qreal x2 = std::abs(fftBuf[3328-i])/winSum;
            if (x1 > x2) {
                if (x2 * 10 > x1) iqSignalFinder[4864+i] = 0;
                else iqSignalFinder[4864+i]++;
            } else {
                if (x1 * 10 > x2) iqSignalFinder[3328-i] = 0;
                else iqSignalFinder[3328-i]++;
            }
        }
        for (auto v : iqSignalFinder) {
            if (v>sigCountThreshold) {
                iqVerifyCount = 0;
                iqBalState = 0;
            }
        }
        return;
    }

    if (iqBalState == 0) {
        // store the current dataset
        pos -= 49152;
        i = 0;
        while (i < 8192) {
            iqRawData[i & 0x1FFF] = (raw[pos] - dcbias) * sincWin[i];
            i++;
            pos++;
        }
        while (i < 49152) {
            iqRawData[i & 0x1FFF] += (raw[pos] - dcbias) * sincWin[i];
            i++;
            pos++;
        }
        iqBalState = 1;
        return;
    }

    // Alternate data-prep and data-analysis each pass
    if (iqBalState & 1) {
        if (iqBalState == 1 && iqVerifyCount) {
            iqPhaseInTest = iqPhase;
            iqGainInTest = iqGain;
        } else if (iqVerifyCount) {
            iqPhaseInTest = iqNewPhase;
            iqGainInTest = iqNewGain;
        } else {
            iqPhaseInTest = iqPhase;
            iqGainInTest = iqGain;
            if (iqBalState > 1) {
                double r;
                // Alternate phase and gain each pass
                if (iqBalState & 2) {
                    r = (double)std::rand() / RAND_MAX - 0.5;
                    r /= 100;
                    iqPhaseInTest += r;
                } else {
                    r = (double)std::rand() / RAND_MAX - 0.5;
                    r /= 200;
                    iqGainInTest += r;
                }
            }
        }
        for (i=0; i<8192; i++) {
            iqDataInTest[i] = std::complex<REAL>(
                                  iqRawData[i].real() + iqPhaseInTest * iqRawData[i].imag(),
                                  iqRawData[i].imag() * iqGainInTest
                              );
        }

        FFT::dft(*(COMPLEX(*)[8192])iqDataInTest.data());
        iqBalState++;
        return;
    }

    qreal delta = 0;
    for (i = 0; i < 2560; ++i) {
        qreal x1, x2;
        if (iqSignalFinder[4864+i] > sigCountThreshold) {
            x1 = 20 * log10(std::abs(iqDataInTest[4864+i])/sincSum);
            x2 = 20 * log10(std::abs(iqDataInTest[3328-i])/sincSum);
        }
        else if (iqSignalFinder[3328-i] > sigCountThreshold) {
            x2 = 20 * log10(std::abs(iqDataInTest[4864+i])/sincSum);
            x1 = 20 * log10(std::abs(iqDataInTest[3328-i])/sincSum);

        }
        else continue;
        double d = x1 - x2;
        if (d < 20 && iqBalState == 1) {
            // Sometimes, a signal vanishes due to lag
            iqSignalFinder[4864+i] = 0;
            iqSignalFinder[3328-i] = 0;
            d = 0;
        }
        delta += d;
    }

    if (iqBalState == 2) {
        // First time through we save the baseline delta
        iqReferenceDelta = delta;
        iqBalState++;
        // Abort when all the signals lag-vanished
        if (delta==0) iqBalState = -1;
        return;
    }
    else if (iqVerifyCount) {
        if (iqReferenceDelta < delta) {
            iqVerifyCount++;
        } else {
            // fails verify
            iqBalState=-1;
            return;
        }
        if (iqVerifyCount > sigCountThreshold) {
            // success!
            iqPhase = iqNewPhase;
            iqGain = iqNewGain;
            iqBalState=-1;
            emit iqBalUpdate(iqPhase, iqGain);
            return;
        }
        return;
    }

    if (iqReferenceDelta < delta) {
        iqNewPhase = iqPhaseInTest;
        iqNewGain = iqGainInTest;
        iqBalState = 0;
        iqVerifyCount = 1;
        return;
    }

    if (iqBalState++ > 30) iqBalState=-1;

}
