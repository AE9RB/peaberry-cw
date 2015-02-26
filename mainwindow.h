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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include "dsp.h"

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(class Radio *radio);

signals:
    void powerValueChanged(int);
    void keyerValueChanged(int);
    void agcValueChanged(qreal);
    void filterValueChanged(int);
    void xitValueChanged(qint64);
    void ritValueChanged(qint64);
    void showSettings();

public slots:
    void setKeyerValue(int wpm);
    void setAgcValue(qreal v);
    void setFilterValue(int v);
    void setSmeter(qreal v);

private slots:
    void powerHandler(int);
    void keyerHandler(int);
    void agcHandler(int);
    void filterHandler(int);
    void freqAdjusted(qreal delta);
    void offsetAdjusted(qreal delta);
    void xitritHandler(int);
    void offsetHandler(qint64 f);
    void setXit(qint64 f);
    void setRit(qint64 f);

private:
    class QSlider *powerSlider;
    class QLabel *powerInfo;
    class QLabel *smeter;
    class QSlider *keyerSlider;
    class QLabel *keyerInfo;
    class QComboBox *agc;
    class QSlider *gain;
    class QComboBox *filter;
    class QComboBox *xitrit;
    class Freq *tuner;
    class Freq *offset;

};

#endif // MAINWINDOW_H
