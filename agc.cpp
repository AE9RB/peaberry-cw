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

#include "agc.h"
#include "radio.h"

// Track two gains which has the effect of looking ahead to make
// sure we do not start to decay for only brief dips in the signal level.
// The basic idea came from dttsp except they seem to
// track the second gain for a different reason.

Agc::Agc(Radio *radio)
{
    attack = exp(-1.0 / (ATTACK * DEMODRATE));
    setDecay(750);

    outPos = ATTACK * 4.0 * DEMODRATE;
    fastPos = outPos - ATTACK * DEMODRATE;
    slowPos = 0;
    slowHangCount = fastHangCount = 0;
    fastGain = slowGain = GAIN_MIN;

    // Compute power of 2 circular buffer size we can bitmask
    buffMask = 1;
    while (buffMask < outPos || buffMask < fastPos) buffMask *= 2;
    buff.resize(buffMask);
    buffMask -= 1;

    connect(radio, SIGNAL(agcSpeedChanged(qreal)), this, SLOT(setDecay(qreal)));
}

void Agc::setDecay(qreal secs)
{
    qreal decays = secs * (1.0-HANG_PERCENT);
    decay = exp(-1.0 / (decays * DEMODRATE));
    qreal hangs = secs * HANG_PERCENT;
    hangTime = hangs * DEMODRATE;
}

void Agc::Process(COMPLEX *data)
{
    for (int i = 0; i < DEMODSIZE; i++) {
        qreal tmp;
        buff[slowPos] = data[i];
        tmp = std::abs(buff[slowPos]);
        if (tmp != 0.0) tmp = 1.0 / tmp;
        else tmp = slowGain;
        if (tmp >= slowGain) {
            if (slowHangCount++ > hangTime) {
                slowGain = decay * slowGain +
                           (1-decay) * std::min(GAIN_MAX, tmp);
            }
        } else {
            slowHangCount = 0;
            slowGain = attack * slowGain +
                       (1 - attack) * std::max(tmp, GAIN_MIN);
        }

        tmp = std::abs(buff[fastPos]);
        if (tmp != 0.0) tmp = 1.0 / tmp;
        else tmp = fastGain;
        if (tmp > fastGain) {
            if (fastHangCount++ > hangTime) {
                fastGain = decay * fastGain +
                           (1-decay) * std::min(GAIN_MAX, tmp);
            }
        } else {
            fastHangCount = 0;
            fastGain = attack * fastGain +
                       (1-attack) * std::max(tmp, GAIN_MIN);
        }
        data[i] = buff[outPos] * (REAL)std::min(fastGain, slowGain);

        slowPos = (slowPos + buffMask) & buffMask;
        outPos = (outPos + buffMask) & buffMask;
        fastPos = (fastPos + buffMask) & buffMask;
    }
}
