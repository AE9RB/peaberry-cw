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

#include "keyer.h"
#include "radio.h"

Keyer::Keyer(Radio *radio, QObject *parent) :
    QObject(parent)
{
    spacing = 2;
    state = 3;
    staged = 0;
    mcode = 0x80;
    k0 = k1 = false;
    mode = Mode::IAMBIC;
    memory = Memory::BOTH;
    cfg_spacing = Spacing::EL;
    reversed = false;
    weight = 0.5;
    read_after = start_after = 0;
    setSpeed(0);

    connect(radio, SIGNAL(speedChanged(int)), this, SLOT(setSpeed(int)));
    connect(radio, SIGNAL(keyWeightChanged(int)), this, SLOT(setWeight(int)));
    connect(radio, SIGNAL(keyModeChanged(int)), this, SLOT(setMode(int)));
    connect(radio, SIGNAL(keyMemoryChanged(int)), this, SLOT(setMemory(int)));
    connect(radio, SIGNAL(keySpacingChanged(int)), this, SLOT(setSpacing(int)));
    connect(radio, SIGNAL(keyReverseChanged(bool)), this, SLOT(setReverse(bool)));
}

void Keyer::setSpeed(int wpm)
{
    speed_wpm = wpm;
    speed_nsec = 1200000000.0 / speed_wpm;
}

void Keyer::setWeight(int w)
{
    weight = w / 100.0;
}

void Keyer::setMode(int m)
{
    mode = (enum Mode)m;
}

void Keyer::setMemory(int m)
{
    memory = (enum Memory)m;
}

void Keyer::setSpacing(int s)
{
    cfg_spacing = (enum Spacing)s;
}

void Keyer::setReverse(bool r)
{
    reversed = r;
}

void Keyer::keyUpdate(bool k0, bool k1)
{
    if (speed_wpm == 0) {
        this->k1 = k0;
        this->k0 = false;
    } else if (reversed) {
        this->k1 = k0;
        this->k0 = k1;
    } else {
        this->k0 = k0;
        this->k1 = k1;
    }
}

qreal Keyer::keyProcess(qint64 mark)
{
    qint64 i;
    quint8 emitcode = 0;
    qreal sendTime = -1;

    switch(state) {
    case 1: // waiting until ready for read
        if (cfg_spacing == Spacing::NONE)
            if ((k0 && last == DIT) || (k1 && last == DAH))
                read_after = mark + KEY_DEBOUNCE_IAMBIC;
        if (read_after - mark < 0) state = 2;
        break;
    case 2: // waiting and reading
        if (start_after - mark < 0) state = 3;
        if (spacing < 4) break;
    //nobreak;
    case 3: // idle, spacing
        if (start_after - mark < 0) {
            switch (spacing) {
            case 0:
            case 2:
            case 3:
                break;
            case 1:
                emitcode = mcode;
                mcode=0x80;
                if (cfg_spacing >= Spacing::CHAR) state = 2;
                break;
            case 4:
                emitcode = mcode;
            //nobreak
            case 5:
            case 6:
                if (cfg_spacing >= Spacing::WORD) state = 2;
                break;
            }
            if (spacing < 7) spacing += 1;
            if (mode == Mode::BUG) state = 3;
            start_after += DIT * speed_nsec;
        }
        break;
    case 4: // debouncing straight/bug down
        if (start_after - mark < 0) state = 5;
        break;
    case 5: // holding straight/bug
        break;
    case 6: // debouncing straight/bug up
        if (read_after - mark < 0) {
            state = 3;
            staged = 0;
            start_after = mark + DIT * speed_nsec;
            spacing = 0;
            if (mcode & 0x01) {
                mcode = 0xFF;
            } else {
                mcode >>= 1;
                mcode |= 0x80;
            }
        }
        break;
    }

    if (speed_wpm == 0 || mode == Mode::BUG) {
        if (k1) {
            i = mark + DEBOUNCE_SRAIGHT;
            if (state < 4) {
                state = 4;
                start_after = i;
            }
            if (state < 6) {
                read_after = i;
                sendTime = 1.0;
            }
            last = DAH;
            staged = 0;
        }
        else if (state == 5) {
            if (staged == DIT) {
                state = 3;
            } else {
                state = 6;
                sendTime = 0.0;
            }
        }
    } else {
        if (state > 3) state = 6;
    }

    if (!staged) {
        if (state > 1) {
            if (k0 && k1) {
                if (ultimatic && mode == Mode::ULTIMATIC) staged = last;
                else if (last == DIT) staged = DAH;
                else staged = DIT;
                ultimatic = 1;
            } else {
                if (k0) staged = DIT;
                if (k1) staged = DAH;
                ultimatic = 0;
            }
        }
        else if (!ultimatic || mode != Mode::ULTIMATIC) {
            if (k0 && (last == DAH || spacing > 0)) {
                if (memory == Memory::DIT || memory == Memory::BOTH) {
                    staged = DIT;
                    ultimatic = 1;
                }
            }
            if (k1 && (last == DIT || spacing > 0)) {
                if (memory == Memory::DAH || memory == Memory::BOTH) {
                    staged = DAH;
                    ultimatic = 1;
                }
            }
        }
    }

    if (state == 3 && staged) {
        i = (qint64)staged * speed_nsec;
        i += DIT * speed_nsec * (weight * 2 - 1);
        read_after = start_after = i;
        sendTime = i/1000000000.0;
        i += mark + DIT * speed_nsec * (2.0 - weight * 2);
        if (cfg_spacing >= Spacing::EL) {
            read_after = i - KEY_DEBOUNCE_IAMBIC;
            start_after = i;
        }
        spacing = 0;
        if (mcode & 0x01) {
            if (mcode != 0x01 || staged==DAH) mcode = 0xFF;
        } else {
            mcode >>= 1;
            if (staged==DAH) mcode |= 0x80;
        }
        last = staged;
        staged = 0;
        state = 1;
    }

    if (speed_wpm != 0) {
        Q_UNUSED(emitcode);
        // for future use
        // can emitCode decoded morse here
    }
    return sendTime;
}
