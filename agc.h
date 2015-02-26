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

#ifndef AGC_H
#define AGC_H

#include <QtCore>
#include "dsp.h"

class Agc : public QObject
{
    Q_OBJECT
public:
    explicit Agc(class Radio *radio);
    ~Agc() {};
    void Process(COMPLEX *data);

public slots:
    void setDecay(qreal secs);

private:
    const qreal ATTACK = 0.002;
    const qreal HANG_PERCENT = 0.40;
    const qreal GAIN_MAX = 100000;
    const qreal GAIN_MIN = 0.0001;
    qreal fastGain;
    qreal slowGain;
    qreal attack;
    qreal decay;
    int hangTime;
    int slowHangCount;
    int fastHangCount;
    int buffMask;
    int slowPos;
    int outPos;
    int fastPos;
    QVector<COMPLEX> buff;
};

#endif // AGC_H
