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

#ifndef KEYER_H
#define KEYER_H

#include <QtCore>

class Keyer : public QObject {
    Q_OBJECT
public:
    explicit Keyer(class Radio *radio, QObject *parent = 0);
    ~Keyer() {}
public slots:
    void setSpeed(int wpm);
    void setWeight(int w);
    void setMode(int m);
    void setMemory(int m);
    void setSpacing(int s);
    void setReverse(bool r);
public:
    void keyUpdate(bool k0, bool k1);
    qreal keyProcess(qint64 mark);
private:
    bool k0;
    bool k1;
    quint8 last, spacing, ultimatic, state, staged, mcode;
    qint64 read_after, start_after;

    enum class Mode : int {
        IAMBIC=0, ULTIMATIC, BUG
    } mode;
    enum class Memory : int {
        NONE=0, DAH, DIT, BOTH
    } memory;
    enum class Spacing : int {
        NONE=0, EL, CHAR, WORD
    } cfg_spacing;
    bool reversed;
    qint64 speed_nsec;
    int speed_wpm;
    qreal weight;
    static const quint8 DIT = 1;
    static const quint8 DAH = 3;
    static const qint64 DEBOUNCE_SRAIGHT = 8000000;
    static const qint64 KEY_DEBOUNCE_IAMBIC = 2000000;
};

#endif // KEYER_H
