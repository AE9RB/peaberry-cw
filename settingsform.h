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

#ifndef SETTINGSFORM_H
#define SETTINGSFORM_H

#include <QWidget>

namespace Ui {
class SettingsForm;
}

class SettingsForm : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsForm(class Radio *radio, QWidget *parent = 0);
    ~SettingsForm();

signals:
    void txPhaseChanged(qreal phase);
    void txGainChanged(qreal gain);

public slots:
    void show();
    void setIqBal(qreal phase, qreal gain);
    void setFreq(qint64 f);
    void setXit(qint64 f);
    void setRit(qint64 f);

    void updatePhase(int v);
    void setPhase(qreal phase);
    void updateGain(int v);
    void setGain(qreal gain);
    void setToneSync(bool b);
    void setRxTone(int hz);

private:
    Ui::SettingsForm *ui;
    bool gotPhaseSet;
    bool gotGainSet;
    qint64 freq;
    qint64 xit;
    qint64 rit;
};

#endif // SETTINGSFORM_H
