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

#ifndef RADIO_H
#define RADIO_H

#include <QtCore>

class Radio : public QObject
{
    Q_OBJECT
public:
    explicit Radio(QObject *parent = 0);
    ~Radio();
    QString error;
    void save();
    void load();
    QSettings* settings() {
        return settings_;
    }

protected:
    class QSettings *settings_;
    bool settingsLoaded;

    QThread ioThread;
    QThread demodThread;
    QThread spectrumThread;

    class Cat *cat;
    class Audio *audio_;
    class Demod *demod;
    class Spectrum *spectrum;

signals: // unsaved glue
    void spectrumViewUpdate(QVector<qreal>*);
    void rxIqBalUpdate(qreal phase, qreal gain);
    void rxDcBiasUpdate(qreal phase, qreal gain);
    void smeterUpdate(qreal);

private:
    int m_speed;
    int m_keyVolume;
    int m_keyWeight;
    int m_keyMode;
    int m_keyMemory;
    int m_keySpacing;
    bool m_keyReverse;
    bool m_toneSync;
    int m_keyTone;
    int m_rxTone;
    int m_colors;
    int m_window;
    qreal m_shape;
    int m_fftSmooth;
    bool m_fftZoom;
    int m_gain;
    qreal m_agcSpeed;
    int m_filter;
    bool m_cwr;
    qint64 m_freq;
    qint64 m_xit;
    qint64 m_rit;
    qreal m_dbOffset;
    int m_power;
    qreal m_txPhase;
    qreal m_txGain;
    int m_qsk;

signals: // saved config
    void speedChanged(int wpm);
    void keyVolumeChanged(int v);
    void keyWeightChanged(int wpm);
    void keyModeChanged(int m);
    void keyMemoryChanged(int m);
    void keySpacingChanged(int s);
    void keyReverseChanged(bool r);
    void toneSyncChanged(bool b);
    void keyToneChanged(int hz);
    void rxToneChanged(int hz);
    void shapeChanged(qreal ms);
    void colorsChanged(int c);
    void windowChanged(int w);
    void fftFilterChanged(int v);
    void fftZoomChanged(bool z);
    void gainChanged(int v);
    void agcSpeedChanged(qreal m);
    void filterChanged(int hz);
    void cwrChanged(bool r);
    void freqChanged(qint64 f);
    void xitChanged(qint64 f);
    void ritChanged(qint64 f);
    void dbOffsetChanged(qreal db);
    void powerChanged(int p);
    void txPhaseChanged(qreal phase);
    void txGainChanged(qreal gain);
    void qskChanged(int ms);

public slots:
    void setSpeed(int wpm);
    void setKeyVolume(int v);
    void setKeyWeight(int w);
    void setKeyMode(int m);
    void setKeyMemory(int m);
    void setKeySpacing(int s);
    void setKeyReverse(bool r);
    void setToneSync(bool b);
    void setKeyTone(int hz);
    void setRxTone(int hz);
    void setShape(qreal ms);
    void setColors(int c);
    void setWindow(int w);
    void setFftFilter(int v);
    void setFftZoom(bool z);
    void setGain(int v);
    void setAgcSpeed(qreal m);
    void setFilter(int hz);
    void setCwr(bool r);
    void setFreq(qint64 f);
    void setXit(qint64 f);
    void setRit(qint64 f);
    void setDbOffset(qreal db);
    void setPower(int p);
    void setTxPhase(qreal phase);
    void setTxGain(qreal gain);
    void setQsk(int ms);
};

#endif // RADIO_H
