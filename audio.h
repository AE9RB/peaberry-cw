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

#ifndef AUDIO_H
#define AUDIO_H

#include <QtCore>
#include "dsp.h"

class AudioBase : public QObject
{
    Q_OBJECT

public:
    QString error;

    inline qreal bufSampleRate() {
        return speakerSampleRate;
    }
    inline qreal bufAverageSize() {
        return speakerBufLevel;
    }
    inline qreal bufAverageTarget() {
        return BUFFER_TARGET * speakerSampleRate;
    }
    inline void bufAppend(qreal v) {
        speakerBuffer[++speakerBufferIn]=v;
    }

protected:
    static constexpr qreal BUFFER_TARGET = 0.030;
    static constexpr qreal MUTE_RAMP_DOWN = 0.0008;
    static constexpr qreal MUTE_RAMP_UP = 0.0015;
    static constexpr int SPECTRUM_FPS = 20;

    explicit AudioBase(class Radio *radio);
    ~AudioBase() {}

    virtual int mutePaddingFrames() {
        return 0;
    }
    virtual qreal transmitPaddingSecs() {
        return 0;
    }
    void updateBufAverageSize(int framesRequested, qreal alpha);

    class Keyer {
    public:
        QAtomicInt samples;
        QAtomicInt shapePos;
        qreal volume;
        QVector<qreal> shape;
        std::complex<qreal> nco;
        std::complex<qreal> clk;
    } speaker, transmit;

    QVector<qreal> speakerBuffer;
    QAtomicInteger<quint16> speakerBufferIn;
    quint16 speakerBufferOut;
    qreal speakerSampleRate;
    qreal speakerBufLevel;

    qreal rxBiasReal;
    qreal rxBiasImag;
    qreal rxIqPhase;
    qreal rxIqGain;

    int txPower;
    qreal txIqPhase;
    qreal txIqGain;
    qint64 xit;
    qint64 rit;

    QVector<COMPLEX> captureRaw;
    QVector<COMPLEX> captureAdj;
    quint16 capturePos;
    int captureCount;

    QAtomicInt receiveMuteCount;
    qreal receiveMuteVolume;
    qreal receiveMuteRampDown;
    qreal receiveMuteRampUp;

    int qskDelay;

signals:
    void spectrumUpdate(COMPLEX *rawData, COMPLEX *adjustedData, quint16 pos);
    void demodUpdate(COMPLEX *data);
    void transmitPaddingUpdate(qreal secs);

public slots:
    void sendElement(qreal keySecs, qreal txSecs);
    void setShape(qreal ms);
    void setQsk(int ms);

    void setRxDcBias(qreal re, qreal im);
    void setRxIqBal(qreal phase, qreal gain);

    void setKeyerVolume(int v);
    void setKeyerTone(int hz);

    void setTransmitPower(int v);
    void setTransmitPhase(qreal phase);
    void setTransmitGain(qreal gain);
    void setXit(qint64 f);
    void setRit(qint64 f);

private:
    void computeTransmitVolume();
    void setTransmitTone();

};

#if defined(Q_OS_WIN)
#include "audio_win.h"
#elif defined(Q_OS_MAC)
#include "audio_osx.h"
#endif

#endif // AUDIO_H
