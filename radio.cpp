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

#include "radio.h"
#include "cat.h"
#include "demod.h"
#include "spectrum.h"
#include "audio.h"
#include <QSettings>

#ifdef Q_OS_WIN32
// do not use registry on Windows
#define SETTINGS_FORMAT QSettings::IniFormat
#else
#define SETTINGS_FORMAT QSettings::NativeFormat
#endif

Radio::Radio(QObject *parent) :
    QObject(parent),
    settingsLoaded(false),
    cat(nullptr),
    demod(nullptr),
    spectrum(nullptr)
{
    settings_ = new QSettings(SETTINGS_FORMAT,
                              QSettings::UserScope,
                              "AE9RB", "PeaberryCW");
    settings_->setFallbacksEnabled(false);

    FastDenormals::enable();
    connect(&ioThread, &QThread::started, &FastDenormals::enable);
    connect(&demodThread, &QThread::started, &FastDenormals::enable);
    connect(&spectrumThread, &QThread::started, &FastDenormals::enable);

    cat = new Cat(this);
    if (!cat->error.isEmpty()) {
        error = cat->error;
        return;
    }
    cat->moveToThread(&ioThread);
    connect(&ioThread, &QThread::started, cat, &Cat::start);
    connect(&ioThread, &QThread::finished, cat, &Cat::stop);
    connect(&ioThread, &QThread::finished, cat, &QObject::deleteLater);

    audio_ = new Audio(this);
    if (!audio_->error.isEmpty()) {
        error = audio_->error;
        return;
    }
    audio_->moveToThread(&ioThread);
    connect(&ioThread, &QThread::started, audio_, &Audio::start);
    connect(&ioThread, &QThread::finished, audio_, &Audio::stop);
    connect(&ioThread, &QThread::finished, audio_, &QObject::deleteLater);

    demod = new Demod(this, audio_);
    demod->moveToThread(&demodThread);
    connect(&demodThread, &QThread::finished, demod, &QObject::deleteLater);

    spectrum = new Spectrum(this);
    spectrum->moveToThread(&spectrumThread);
    connect(&spectrumThread, &QThread::finished, spectrum, &QObject::deleteLater);

    // Critical signals that we don't want to pass through UI queue
    connect(cat, SIGNAL(sendElement(qreal,qreal)), audio_, SLOT(sendElement(qreal,qreal)));
    connect(audio_, SIGNAL(spectrumUpdate(COMPLEX*,COMPLEX*,quint16)),
            spectrum, SLOT(spectrumUpdate(COMPLEX*,COMPLEX*,quint16)));
    connect(audio_, SIGNAL(demodUpdate(COMPLEX*)), demod, SLOT(demod(COMPLEX*)));
    connect(audio_, SIGNAL(transmitPaddingUpdate(qreal)),
            cat, SLOT(setTransmitPadding(qreal)));

    demodThread.start(QThread::HighestPriority);
    spectrumThread.start(QThread::HighPriority);
    ioThread.start(QThread::HighestPriority);
}

Radio::~Radio()
{
    save();
    spectrumThread.quit();
    spectrumThread.wait();
    demodThread.quit();
    demodThread.wait();
    ioThread.quit();
    ioThread.wait();
}

void Radio::save()
{
    while (!settings_->group().isEmpty()) settings_->endGroup();
    if (settingsLoaded) {
        settings_->setValue("speed", m_speed);
        settings_->setValue("keyVolume", m_keyVolume);
        settings_->setValue("keyWeight", m_keyWeight);
        settings_->setValue("keyMode", m_keyMode);
        settings_->setValue("keyMemory", m_keyMemory);
        settings_->setValue("keySpacing", m_keySpacing);
        settings_->setValue("keyReverse", m_keyReverse);
        settings_->setValue("toneSync", m_toneSync);
        settings_->setValue("keyTone", m_keyTone);
        settings_->setValue("rxTone", m_rxTone);
        settings_->setValue("shape", m_shape);
        settings_->setValue("colors", m_colors);
        settings_->setValue("window", m_window);
        settings_->setValue("fftFilter", m_fftSmooth);
        settings_->setValue("fftZoom", m_fftZoom);
        settings_->setValue("gain", m_gain);
        settings_->setValue("agcSpeed", m_agcSpeed);
        settings_->setValue("filter", m_filter);
        settings_->setValue("cwr", m_cwr);
        settings_->setValue("qsk", m_qsk);
        // Begin radio-specific settings
        settings_->beginGroup(cat->serialNumber);
        settings_->setValue("freq", m_freq);
        settings_->setValue("xit", m_xit);
        settings_->setValue("rit", m_rit);
        settings_->setValue("dbOffset", m_dbOffset);
        settings_->setValue("power", m_power);
        settings_->setValue("phase", m_txPhase);
        settings_->setValue("gain", m_txGain);
        settings_->endGroup();
        settings_->sync();
    }
}

void Radio::load()
{
    int tmpInt;
    double tmpDouble;
    bool tmpBool;

    while (!settings_->group().isEmpty()) settings_->endGroup();

    tmpInt = settings_->value("speed", 15).toInt();
    m_speed = tmpInt + 1;
    setSpeed(tmpInt);

    tmpInt = settings_->value("keyVolume", 75).toInt();
    m_keyVolume = tmpInt + 1;
    setKeyVolume(tmpInt);

    tmpInt = settings_->value("keyWeight", 50).toInt();
    m_keyWeight = tmpInt + 1;
    setKeyWeight(tmpInt);

    tmpInt = settings_->value("keyMode", 0).toInt();
    m_keyMode = tmpInt + 1;
    setKeyMode(tmpInt);

    tmpInt = settings_->value("keyMemory", 3).toInt();
    m_keyMemory = tmpInt + 1;
    setKeyMemory(tmpInt);

    tmpInt = settings_->value("keySpacing", 1).toInt();
    m_keySpacing = tmpInt + 1;
    setKeySpacing(tmpInt);

    tmpBool = settings_->value("keyReverse", false).toBool();
    m_keyReverse = !tmpBool;
    setKeyReverse(tmpBool);

    tmpBool = settings_->value("toneSync", true).toBool();
    m_toneSync = !tmpBool;
    setToneSync(tmpBool);

    tmpInt = settings_->value("keyTone", 600).toInt();
    m_keyTone = tmpInt + 1;
    setKeyTone(tmpInt);

    tmpInt = settings_->value("rxTone", 600).toInt();
    m_rxTone = tmpInt + 1;
    setRxTone(tmpInt);

    tmpDouble = settings_->value("shape", 4.0).toDouble();
    m_shape = tmpDouble + 1;
    setShape(tmpDouble);

    tmpInt = settings_->value("colors", 1).toInt();
    m_colors = tmpInt + 1;
    setColors(tmpInt);

    tmpInt = settings_->value("window", 0).toInt();
    m_window = tmpInt + 1;
    setWindow(tmpInt);

    tmpInt = settings_->value("fftFilter", 50).toInt();
    m_fftSmooth = tmpInt + 1;
    setFftFilter(tmpInt);

    tmpBool = settings_->value("fftZoom", false).toBool();
    m_fftZoom = !tmpBool;
    setFftZoom(tmpBool);

    tmpInt = settings_->value("gain", 50).toInt();
    m_gain = tmpInt + 1;
    setGain(tmpInt);

    tmpDouble = settings_->value("agcSpeed", -1).toDouble();
    m_agcSpeed = tmpDouble + 1;
    setAgcSpeed(tmpDouble);

    tmpInt = settings_->value("filter", 500).toInt();
    m_filter = tmpInt + 1;
    setFilter(tmpInt);

    tmpBool = settings_->value("cwr", false).toBool();
    m_cwr = !tmpBool;
    setCwr(tmpBool);

    tmpInt = settings_->value("qsk", 0).toInt();
    m_qsk = tmpInt + 1;
    setQsk(tmpInt);

    // Begin radio-specific settings
    settings_->beginGroup(cat->serialNumber);

    tmpInt = settings_->value("freq", 14060000).toInt();
    m_freq = tmpInt + 1;
    setFreq(tmpInt);

    tmpInt = settings_->value("xit", 0).toInt();
    m_xit = tmpInt + 1;
    setXit(tmpInt);

    tmpInt = settings_->value("rit", 0).toInt();
    m_rit = tmpInt + 1;
    setRit(tmpInt);

    tmpDouble = settings_->value("dbOffset", 0.0).toDouble();
    m_dbOffset = tmpDouble + 1;
    setDbOffset(tmpDouble);

    tmpInt = settings_->value("power", 100).toInt();
    m_power = tmpInt + 1;
    setPower(tmpInt);

    tmpDouble = settings_->value("phase", 0.0).toDouble();
    m_txPhase = tmpDouble + 1;
    setTxPhase(tmpDouble);

    tmpDouble = settings_->value("gain", 1.0).toDouble();
    m_txGain = tmpDouble + 1;
    setTxGain(tmpDouble);

    settings_->endGroup();

    settingsLoaded = true;
}

void Radio::setSpeed(int wpm)
{
    bool changed = (m_speed != wpm);
    m_speed = wpm;
    if (changed) emit(speedChanged(wpm));
}

void Radio::setKeyVolume(int v)
{
    bool changed = (m_keyVolume != v);
    m_keyVolume = v;
    if (changed) emit(keyVolumeChanged(v));
}

void Radio::setKeyWeight(int w)
{
    bool changed = (m_keyWeight != w);
    m_keyWeight = w;
    if (changed) emit(keyWeightChanged(w));
}

void Radio::setKeyMode(int m)
{
    bool changed = (m_keyMode != m);
    m_keyMode = m;
    if (changed) emit(keyModeChanged(m));
}

void Radio::setKeyMemory(int m)
{
    bool changed = (m_keyMemory != m);
    m_keyMemory = m;
    if (changed) emit(keyMemoryChanged(m));
}

void Radio::setKeySpacing(int s)
{
    bool changed = (m_keySpacing != s);
    m_keySpacing = s;
    if (changed) emit(keySpacingChanged(s));
}

void Radio::setKeyReverse(bool r)
{
    bool changed = (m_keyReverse != r);
    m_keyReverse = r;
    if (changed) emit(keyReverseChanged(r));
}

void Radio::setToneSync(bool b)
{
    bool changed = (m_toneSync != b);
    m_toneSync = b;
    if (changed) emit(toneSyncChanged(b));
}

void Radio::setKeyTone(int hz)
{
    bool changed = (m_keyTone != hz);
    m_keyTone = hz;
    if (changed) emit(keyToneChanged(hz));
}

void Radio::setRxTone(int hz)
{
    bool changed = (m_rxTone != hz);
    m_rxTone = hz;
    if (changed) emit(rxToneChanged(hz));
}

void Radio::setShape(qreal ms)
{
    bool changed = (m_shape != ms);
    m_shape = ms;
    if (changed) emit(shapeChanged(ms));
}

void Radio::setColors(int c)
{
    bool changed = (m_colors != c);
    m_colors = c;
    if (changed) emit(colorsChanged(c));
}

void Radio::setWindow(int w)
{
    bool changed = (m_window != w);
    m_window = w;
    if (changed) emit(windowChanged(w));
}

void Radio::setFftFilter(int v)
{
    bool changed = (m_fftSmooth != v);
    m_fftSmooth = v;
    if (changed) emit(fftFilterChanged(v));
}

void Radio::setFftZoom(bool z)
{
    bool changed = (m_fftZoom != z);
    m_fftZoom = z;
    if (changed) emit(fftZoomChanged(z));
}

void Radio::setGain(int v)
{
    bool changed = (m_gain != v);
    m_gain = v;
    if (changed) emit(gainChanged(v));
}

void Radio::setAgcSpeed(qreal m)
{
    bool changed = (m_agcSpeed != m);
    m_agcSpeed = m;
    if (changed) emit(agcSpeedChanged(m));
}

void Radio::setFilter(int hz)
{
    bool changed = (m_filter != hz);
    m_filter = hz;
    if (changed) emit(filterChanged(hz));
}

void Radio::setCwr(bool r)
{
    bool changed = (m_cwr != r);
    m_cwr = r;
    if (changed) emit(cwrChanged(r));
}

void Radio::setFreq(qint64 f)
{
    bool changed = (m_freq != f);
    m_freq = f;
    if (changed) emit(freqChanged(f));
}

void Radio::setXit(qint64 f)
{
    bool changed = (m_xit != f);
    m_xit = f;
    if (changed) emit(xitChanged(f));
}

void Radio::setRit(qint64 f)
{
    bool changed = (m_rit != f);
    m_rit = f;
    if (changed) emit(ritChanged(f));
}

void Radio::setDbOffset(qreal db)
{
    bool changed = (m_dbOffset != db);
    m_dbOffset = db;
    if (changed) emit(dbOffsetChanged(db));
}

void Radio::setPower(int p)
{
    bool changed = (m_power != p);
    m_power = p;
    if (changed) emit(powerChanged(p));
}

void Radio::setTxPhase(qreal phase)
{
    bool changed = (m_txPhase != phase);
    m_txPhase = phase;
    if (changed) emit(txPhaseChanged(phase));
}

void Radio::setTxGain(qreal gain)
{
    bool changed = (m_txGain != gain);
    m_txGain = gain;
    if (changed) emit(txGainChanged(gain));
}

void Radio::setQsk(int ms)
{
    bool changed = (m_qsk != ms);
    m_qsk = ms;
    if (changed) emit(qskChanged(ms));
}
