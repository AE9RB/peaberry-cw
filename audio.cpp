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

#include "audio.h"
#include "radio.h"

AudioBase::AudioBase(Radio *radio) :
    speakerBuffer(65536),
    captureRaw(65536),
    captureAdj(65536)
{
    speaker.samples = 0;
    speaker.shapePos = 0;
    transmit.samples = 0;
    transmit.shapePos = 0;
    speakerSampleRate = DEMODRATE;

    setRxIqBal(0,1);
    setRxDcBias(0,0);

    setShape(6);

    setKeyerTone(600);
    setKeyerVolume(99);

    xit = rit = 0;
    setTransmitTone();
    setTransmitPower(100);
    setTransmitPhase(0);
    setTransmitGain(1);

    speakerBufferIn = speakerBufferOut = speakerBufLevel = 0;
    captureCount = capturePos = 0;
    receiveMuteCount = 0;
    receiveMuteVolume = 0;
    receiveMuteRampDown = exp(-1.0 / (MUTE_RAMP_DOWN * PEABERRYRATE));
    receiveMuteRampUp = exp(-1.0 / (MUTE_RAMP_UP * PEABERRYRATE));

    connect(radio, SIGNAL(powerChanged(int)), this, SLOT(setTransmitPower(int)));
    connect(radio, SIGNAL(keyVolumeChanged(int)), this, SLOT(setKeyerVolume(int)));
    connect(radio, SIGNAL(keyToneChanged(int)), this, SLOT(setKeyerTone(int)));
    connect(radio, SIGNAL(shapeChanged(double)), this, SLOT(setShape(double)));
    connect(radio, SIGNAL(qskChanged(int)), this, SLOT(setQsk(int)));
    connect(radio, SIGNAL(rxIqBalUpdate(qreal,qreal)), this, SLOT(setRxIqBal(qreal,qreal)));
    connect(radio, SIGNAL(rxDcBiasUpdate(qreal,qreal)), this, SLOT(setRxDcBias(qreal,qreal)));
    connect(radio, SIGNAL(txGainChanged(qreal)), this, SLOT(setTransmitGain(qreal)));
    connect(radio, SIGNAL(txPhaseChanged(qreal)), this, SLOT(setTransmitPhase(qreal)));
    connect(radio, SIGNAL(ritChanged(qint64)), this, SLOT(setRit(qint64)));
    connect(radio, SIGNAL(xitChanged(qint64)), this, SLOT(setXit(qint64)));
}

void AudioBase::updateBufAverageSize(int framesRequested, qreal alpha)
{
    int bufSize;
    if (speakerBufferIn >= speakerBufferOut) bufSize = speakerBufferIn - speakerBufferOut;
    else bufSize = (int)speakerBufferIn + 65536 - (int)speakerBufferOut;

    #ifdef QT_DEBUG
    static int supress = 1000;
    if (!supress) {
        if (bufSize < framesRequested) {
            qWarning() << "UNDERRUN bufAverageSize =" << bufAverageSize();
            supress += 50;
        }
    } else supress--;
    #endif

    if (bufSize > bufAverageTarget() * 2.5) {
        speakerBufferIn = speakerBufferOut + bufAverageTarget();
        #ifdef QT_DEBUG
        if (!supress) {
            qWarning() << "OVERRUN bufAverageSize =" << bufAverageSize();
            supress += 50;
        }
        #endif
    }
    speakerBufLevel = (1.0-alpha)*speakerBufLevel + alpha*bufSize;
    while (bufSize < framesRequested) {
        bufSize++;
        speakerBuffer[speakerBufferOut--] = 0;
    }
}

void AudioBase::sendElement(qreal keySecs, qreal txSecs)
{
    // Rounding errors will accumulate.
    // Reset nco whenever we start a new tone.
    if (!speaker.samples && !speaker.shapePos) {
        speaker.nco = std::complex<qreal>(0.0, 1.0);
    }
    speaker.samples = keySecs * speakerSampleRate;

    if (txSecs >= 0) {
        if (!transmit.samples && !transmit.shapePos) {
            transmit.nco = std::complex<qreal>(0.0, 1.0);
        }
        transmit.samples = txSecs * PEABERRYRATE;
        receiveMuteCount = txSecs * PEABERRYRATE +
                           mutePaddingFrames() +
                           transmit.shape.size() / 2 +
                           qskDelay / 1000.0 * PEABERRYRATE;
    }
}

void AudioBase::setShape(qreal ms)
{
    int size, i;

    size = ms / 1000 * speakerSampleRate;
    speaker.shape.resize(size);
    i = 1;
    for (auto &v : speaker.shape) {
        v = 0.5 - 0.5 * cos(M_PI*i/(size+1));
        ++i;
    }
    if (speaker.shapePos) speaker.shapePos = size;
    else speaker.shapePos = 0;

    size = ms / 1000 * PEABERRYRATE;
    transmit.shape.resize(size);
    i = 1;
    for (auto &v : transmit.shape) {
        v = 0.5 - 0.5 * cos(M_PI*i/(size+1));
        ++i;
    }
    if (transmit.shapePos) transmit.shapePos = size;
    else transmit.shapePos = 0;

    setQsk(qskDelay); // for transmitPaddingUpdate
}

void AudioBase::setQsk(int ms)
{
    qskDelay = ms;
    emit transmitPaddingUpdate(transmitPaddingSecs()
                               + transmit.shape.size() / (qreal)PEABERRYRATE / 2
                               + qskDelay / 1000.0);
}

void AudioBase::setRxDcBias(qreal re, qreal im)
{
    rxBiasReal = re;
    rxBiasImag = im;
}

void AudioBase::setRxIqBal(qreal phase, qreal gain)
{
    rxIqPhase = phase;
    rxIqGain = gain;
}

void AudioBase::setKeyerVolume(int v)
{
    const qreal full_volume = 99;
    speaker.volume = 0.5 * (1 - log10((full_volume / 9 + full_volume - v) * 9 / full_volume));
}

void AudioBase::setKeyerTone(int hz)
{
    qreal inc = 2 * M_PI * hz / speakerSampleRate;
    speaker.clk = std::complex<qreal>(cos(inc), sin(inc));
}

void AudioBase::setTransmitPower(int v)
{
    txPower = v;
    computeTransmitVolume();
}

void AudioBase::setTransmitTone()
{
    qreal inc = 2 * M_PI * (-24000 + xit - rit) / PEABERRYRATE;
    transmit.clk = std::complex<qreal>(cos(inc), sin(inc));
}

void AudioBase::setTransmitPhase(qreal phase)
{
    txIqPhase = phase;
    computeTransmitVolume();
}

void AudioBase::setTransmitGain(qreal gain)
{
    txIqGain = gain;
    computeTransmitVolume();
}

void AudioBase::setXit(qint64 f)
{
    xit = f;
    setTransmitTone();
}

void AudioBase::setRit(qint64 f)
{
    rit = f;
    setTransmitTone();
}

void AudioBase::computeTransmitVolume()
{
    const qreal full_volume = 100;
    // Maximize power.
    // If we had to support any waveform, we'd have to test phase with:
    // qreal p = 1.0/(1.0+fabs(txIqPhase));
    qreal g = 1.0/fabs(txIqGain);
    transmit.volume = std::min(1.0,g) *
                      (1 - log10((full_volume / 9 + full_volume - txPower) * 9 / full_volume));
}
