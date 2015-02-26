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

#include "demod.h"
#include "radio.h"
#include "audio.h"
#include "agc.h"
#include "fft.h"

Demod::Demod(Radio *radio, class Audio *audio) :
    radio(radio),
    audio(audio),
    ovsvFilter(DEMODSIZE*2),
    ovsvWork(DEMODSIZE*2),
    tempData(DEMODSIZE*2)
{
    agc = new Agc(radio);

    filterWidth = 500;
    tone = 600;
    cwr = false;
    gain = 0;
    ovsvOsc = 1;
    configureFirOvSvMixer();
    configureFirOvSvFilter();

    madOsc = 1;
    madPos = 0;
    qreal inc = 2.0 * M_PI * 24000.0 / PEABERRYRATE;
    madInc = std::complex<qreal>(cos(inc), sin(inc));

    setupResampler();

    connect(radio, SIGNAL(gainChanged(int)), this, SLOT(setGain(int)));
    connect(radio, SIGNAL(filterChanged(int)), this, SLOT(setFilter(int)));
    connect(radio, SIGNAL(cwrChanged(bool)), this, SLOT(setCwr(bool)));
    connect(radio, SIGNAL(rxToneChanged(int)), this, SLOT(setTone(int)));
}

Demod::~Demod()
{
    delete agc;
}

void Demod::setGain(int v)
{
    const qreal full_volume = 99;
    gain = 0.5 * (1 - log10((full_volume / 9 + full_volume - v) * 9 / full_volume));
}

void Demod::setFilter(int hz)
{
    filterWidth = hz;
    configureFirOvSvFilter();
}

void Demod::setTone(int hz)
{
    tone = hz;
    configureFirOvSvMixer();
}

void Demod::setCwr(bool r)
{
    cwr = r;
    configureFirOvSvMixer();
}

void Demod::demod(COMPLEX *data)
{
    // cpxData is double sized. Used the second half for work.
    // First half is used for resampler to keep an overlap.
    mixAndDecimate(data, &tempData[DEMODSIZE]);
    firOvSv(&tempData[DEMODSIZE], &tempData[DEMODSIZE]);
    agc->Process(&tempData[DEMODSIZE]);
    resample(&tempData[DEMODSIZE-RESAMPLE_SINC_SIZE]);
}

void Demod::mixAndDecimate(COMPLEX *inData, COMPLEX *outData)
{
    // 11-tap LPF designed by Moe Wheatley AE4JY
    static const REAL FIR0 = 0.0060431029837374152;
    static const REAL FIR2 = -0.049372515458761493;
    static const REAL FIR4 = 0.29332944952052842;
    static const REAL FIR5 = 0.5;

    // Remove rounding errors in clock
    qreal gain = 2.0 - (madOsc.real()*madOsc.real() + madOsc.imag()*madOsc.imag());
    madOsc = std::complex<qreal>(madOsc.real()*gain, madOsc.imag()*gain);

    int o = 0;
    for (int i = 0; i < PEABERRYSIZE; i++) {
        if (!madPos) madPos = 10;
        else --madPos;
        madBuf[madPos] = madBuf[madPos+11] = inData[i] * madOsc;
        madOsc *= madInc;
        if (!madPos) madPos = 10;
        else --madPos;
        i++;
        madBuf[madPos] = madBuf[madPos+11] = inData[i] * madOsc;
        madOsc *= madInc;
        outData[o++] = std::complex<REAL>(
                           FIR0*madBuf[madPos].real() + FIR2*madBuf[madPos+2].real() +
                           FIR4*madBuf[madPos+4].real() + FIR5*madBuf[madPos+5].real() +
                           FIR4*madBuf[madPos+6].real() + FIR2*madBuf[madPos+8].real() +
                           FIR0*madBuf[madPos+10].real(),
                           FIR0*madBuf[madPos].imag() + FIR2*madBuf[madPos+2].imag() +
                           FIR4*madBuf[madPos+4].imag() + FIR5*madBuf[madPos+5].imag() +
                           FIR4*madBuf[madPos+6].imag() + FIR2*madBuf[madPos+8].imag() +
                           FIR0*madBuf[madPos+10].imag()
                       );
    }
}

void Demod::configureFirOvSvMixer()
{
    qreal signedTone;
    if (cwr) signedTone = tone;
    else signedTone = -tone;
    qreal inc = 2.0 * M_PI * signedTone / DEMODRATE;
    ovsvInc = std::complex<qreal>(cos(inc), sin(inc));
}

void Demod::configureFirOvSvFilter()
{
    int size = ovsvFilter.size() / 2 + 1;
    int midpoint = ovsvFilter.size() / 4;
    qreal fc = filterWidth / 2.0 / DEMODRATE;
    for (int i = size; i < ovsvFilter.size(); i++) ovsvFilter[i] = 0;
    for (int i = 0; i < size; i++) {
        if (i == midpoint)
            ovsvFilter[i] = 2.0 * fc / (DEMODSIZE*2);
        else {
            ovsvFilter[i] =
                // low pass filter
                sin(2.0 * M_PI * fc * (i - midpoint)) /
                (M_PI * (i - midpoint))
                // Blackman-Harris window
                * (0.35875 -
                   0.48829 * cos (2.0 * M_PI * i / (size - 1)) +
                   0.14128 * cos (4.0 * M_PI * i / (size - 1)) -
                   0.01168 * cos (6.0 * M_PI * i / (size - 1)))
                // bake in FFT scaling
                / (DEMODSIZE*2);
        }
    }
    FFT::idft(*(COMPLEX(*)[DEMODSIZE*2])ovsvFilter.data());
}

// Using a low pass fir filter and mixing into position.
void Demod::firOvSv(COMPLEX *inData, COMPLEX *outData)
{
    // Load new data and process
    for (int i = 0; i < DEMODSIZE; i++) ovsvWork[i+DEMODSIZE] = inData[i];
    FFT::idft(*(COMPLEX(*)[DEMODSIZE*2])ovsvWork.data());
    for (int i = 0; i < DEMODSIZE * 2; i++) ovsvWork[i] *= ovsvFilter[i];
    FFT::dft(*(COMPLEX(*)[DEMODSIZE*2])ovsvWork.data());
    // Remove rounding errors in clock
    qreal gain = 2.0 - (ovsvOsc.real()*ovsvOsc.real() + ovsvOsc.imag()*ovsvOsc.imag());
    ovsvOsc = std::complex<qreal>(ovsvOsc.real()*gain, ovsvOsc.imag()*gain);
    // Save overlap and mix output
    for (int i = 0; i < DEMODSIZE; i++) {
        ovsvWork[i] = inData[i];
        outData[i] = ovsvWork[i+DEMODSIZE] * ovsvOsc;
        ovsvOsc *= ovsvInc;
    }
}

void Demod::setupResampler()
{
    const int size = RESAMPLE_SINC_SIZE * RESAMPLE_POSITIONS;
    resampleTable.resize(size);
    for(int i=0; i < size; i++) {
        qreal x = M_PI * (i - size / 2) / RESAMPLE_POSITIONS ;
        // convolve so resampling uses sequential memory access
        int pos = (i / RESAMPLE_POSITIONS) + (i % RESAMPLE_POSITIONS) * RESAMPLE_SINC_SIZE;
        if (i == size/2)
            resampleTable[pos] = 1.0;
        else
            resampleTable[pos] = sin(x) / x
                                 // Blackman-Harris window
                                 * (0.35875 -
                                    0.48829 * cos(2.0 * M_PI * i / (size-1)) +
                                    0.14128 * cos(4.0 * M_PI * i / (size-1)) -
                                    0.01168 * cos(6.0 * M_PI * i / (size-1)));
    }
    resamplePos = 0;
    resampleRate = (qreal)DEMODRATE / audio->bufSampleRate();
}

void Demod::resample(COMPLEX *inData)
{
    int tablePos, inPos = resamplePos;
    while (inPos < DEMODSIZE) {
        // Find the proper sinc filter for our position in time
        if (inPos == resamplePos) tablePos = 0;
        else tablePos = ((qreal)inPos - resamplePos + 1) * RESAMPLE_POSITIONS ;
        tablePos *= RESAMPLE_SINC_SIZE;
        // Compute new sample from impulse response
        qreal sample = 0.0;
        for (int i=0; i < RESAMPLE_SINC_SIZE; i++) {
            sample += (inData[inPos+i].real() * resampleTable[tablePos+i] );
        }
        // Store result in the audio output circular buffer
        audio->bufAppend(sample * gain);
        resamplePos += resampleRate;
        inPos = resamplePos;
    }
    resamplePos -= DEMODSIZE;

    // Preserve overlap for next pass
    for (int i = 0; i < RESAMPLE_SINC_SIZE; i++) {
        inData[i] = inData[i+DEMODSIZE];
    }
    // recompute the rate correction
    qreal rateGain = 5e-7 * (audio->bufSampleRate() / (qreal)DEMODRATE);
    resampleRate = (qreal)DEMODRATE / audio->bufSampleRate() *
                   (1 + rateGain * (audio->bufAverageSize() -  audio->bufAverageTarget()));

    // Debug helper for tuning constants
    //static int debugCount = 0;
    //if (!debugCount--) {
    //    debugCount = 20;
    //    qDebug() << audio->bufAverageSize() << audio->bufAverageTarget() << resampleRate;
    //}
}
