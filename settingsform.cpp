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

#include "settingsform.h"
#include "ui_settingsform.h"

#include "radio.h"
#include <QDebug>

SettingsForm::SettingsForm(Radio *radio, QWidget *parent) :
    QWidget(parent, Qt::Dialog |
            Qt::CustomizeWindowHint |
            Qt::WindowTitleHint |
            Qt::WindowCloseButtonHint),
    ui(new Ui::SettingsForm)
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("Peaberry CW Settings"));
    setAttribute(Qt::WA_QuitOnClose, false);
    ui->versionLabel->setText(QStringLiteral("Version " VER));
    resize(0,0);

    connect(radio, SIGNAL(keyVolumeChanged(int)), ui->keyVolumeSlider, SLOT(setValue(int)));
    connect(ui->keyVolumeSlider, SIGNAL(valueChanged(int)), radio, SLOT(setKeyVolume(int)));

    connect(radio, SIGNAL(keyWeightChanged(int)), ui->keyWeightSlider, SLOT(setValue(int)));
    connect(ui->keyWeightSlider, SIGNAL(valueChanged(int)), radio, SLOT(setKeyWeight(int)));

    connect(radio, SIGNAL(keyModeChanged(int)), ui->keyModeComboBox, SLOT(setCurrentIndex(int)));
    connect(ui->keyModeComboBox, SIGNAL(currentIndexChanged(int)), radio, SLOT(setKeyMode(int)));

    connect(radio, SIGNAL(rxToneChanged(int)), this, SLOT(setRxTone(int)));
    connect(ui->rxToneSpinBox, SIGNAL(valueChanged(int)), radio, SLOT(setRxTone(int)));

    connect(radio, SIGNAL(keyToneChanged(int)), ui->keyToneSpinBox, SLOT(setValue(int)));
    connect(ui->keyToneSpinBox, SIGNAL(valueChanged(int)), radio, SLOT(setKeyTone(int)));

    connect(radio, SIGNAL(shapeChanged(double)), ui->shapeSpinBox, SLOT(setValue(double)));
    connect(ui->shapeSpinBox, SIGNAL(valueChanged(double)), radio, SLOT(setShape(double)));

    connect(radio, SIGNAL(colorsChanged(int)), ui->colorsComboBox, SLOT(setCurrentIndex(int)));
    connect(ui->colorsComboBox, SIGNAL(currentIndexChanged(int)), radio, SLOT(setColors(int)));

    connect(radio, SIGNAL(fftFilterChanged(int)), ui->fftFilterSlider, SLOT(setValue(int)));
    connect(ui->fftFilterSlider, SIGNAL(valueChanged(int)), radio, SLOT(setFftFilter(int)));

    connect(radio, SIGNAL(keyMemoryChanged(int)), ui->keyMemoryComboBox, SLOT(setCurrentIndex(int)));
    connect(ui->keyMemoryComboBox, SIGNAL(currentIndexChanged(int)), radio, SLOT(setKeyMemory(int)));

    connect(radio, SIGNAL(keySpacingChanged(int)), ui->keySpacingComboBox, SLOT(setCurrentIndex(int)));
    connect(ui->keySpacingComboBox, SIGNAL(currentIndexChanged(int)), radio, SLOT(setKeySpacing(int)));

    connect(radio, SIGNAL(keyReverseChanged(bool)), ui->keyReverseCheckBox, SLOT(setChecked(bool)));
    connect(ui->keyReverseCheckBox, SIGNAL(toggled(bool)), radio, SLOT(setKeyReverse(bool)));

    connect(radio, SIGNAL(cwrChanged(bool)), ui->cwrCheckBox, SLOT(setChecked(bool)));
    connect(ui->cwrCheckBox, SIGNAL(toggled(bool)), radio, SLOT(setCwr(bool)));

    connect(radio, SIGNAL(dbOffsetChanged(qreal)), ui->dbOffsetSpinBox, SLOT(setValue(qreal)));
    connect(ui->dbOffsetSpinBox, SIGNAL(valueChanged(qreal)), radio, SLOT(setDbOffset(qreal)));

    connect(radio, SIGNAL(windowChanged(int)), ui->windowComboBox, SLOT(setCurrentIndex(int)));
    connect(ui->windowComboBox, SIGNAL(currentIndexChanged(int)), radio, SLOT(setWindow(int)));

    connect(radio, SIGNAL(rxIqBalUpdate(qreal,qreal)), this, SLOT(setIqBal(qreal,qreal)));

    connect(radio, SIGNAL(freqChanged(qint64)), this, SLOT(setFreq(qint64)));
    connect(radio, SIGNAL(xitChanged(qint64)), this, SLOT(setXit(qint64)));
    connect(radio, SIGNAL(ritChanged(qint64)), this, SLOT(setRit(qint64)));

    connect(ui->phaseCoarseSlider, SIGNAL(valueChanged(int)), this, SLOT(updatePhase(int)));
    connect(ui->phaseFineSlider, SIGNAL(valueChanged(int)), this, SLOT(updatePhase(int)));
    connect(this, SIGNAL(txPhaseChanged(qreal)), radio, SLOT(setTxPhase(qreal)));
    connect(radio, SIGNAL(txPhaseChanged(qreal)), this, SLOT(setPhase(qreal)));
    gotPhaseSet = false;

    connect(ui->gainCoarseSlider, SIGNAL(valueChanged(int)), this, SLOT(updateGain(int)));
    connect(ui->gainFineSlider, SIGNAL(valueChanged(int)), this, SLOT(updateGain(int)));
    connect(this, SIGNAL(txGainChanged(qreal)), radio, SLOT(setTxGain(qreal)));
    connect(radio, SIGNAL(txGainChanged(qreal)), this, SLOT(setGain(qreal)));
    gotGainSet = false;

    connect(radio, SIGNAL(toneSyncChanged(bool)), this, SLOT(setToneSync(bool)));
    connect(ui->keyToneCheckBox, SIGNAL(toggled(bool)), radio, SLOT(setToneSync(bool)));

    connect(radio, SIGNAL(qskChanged(int)), ui->qskSpinBox, SLOT(setValue(int)));
    connect(ui->qskSpinBox, SIGNAL(valueChanged(int)), radio, SLOT(setQsk(int)));
}

SettingsForm::~SettingsForm()
{
    delete ui;
}

void SettingsForm::show()
{
    QWidget::show();
    setFixedSize(size());
    raise();
    activateWindow();
}

void SettingsForm::setIqBal(qreal phase, qreal gain)
{
    ui->phaseLabel->setText(QString::number(phase));
    ui->gainLabel->setText(QString::number(gain));
}

void SettingsForm::setFreq(qint64 f)
{
    freq = f;
    QString s("Image: %L1 Hz");
    ui->txImageBox->setTitle(s.arg(f+48000-xit+(rit*2)));
}

void SettingsForm::setXit(qint64 f)
{
    xit = f;
    setFreq(freq);
}

void SettingsForm::setRit(qint64 f)
{
    rit = f;
    setFreq(freq);
}

void SettingsForm::updatePhase(int v)
{
    Q_UNUSED(v);
    qreal p;
    p = ui->phaseCoarseSlider->value() / 100.0;
    p += ui->phaseFineSlider->value() / 5000.0;
    QString s("Phase: %1");
    ui->phaseBox->setTitle(s.arg(p));
    emit txPhaseChanged(tan(p * M_PI / 180.0));
}

void SettingsForm::setPhase(qreal phase)
{
    if (gotPhaseSet) return;
    gotPhaseSet = true;
    qreal p = atan(phase) * 180 / M_PI;
    int coarse = p * 100;
    int fine = (p-coarse/100.0) * 5000;
    ui->phaseCoarseSlider->setValue(coarse);
    ui->phaseFineSlider->setValue(fine);
}

void SettingsForm::updateGain(int v)
{
    Q_UNUSED(v);
    qreal g;
    g = ui->gainCoarseSlider->value() / 5000.0;
    g += ui->gainFineSlider->value() / 100000.0;
    g += 1;
    QString s("Gain: %1");
    ui->gainBox->setTitle(s.arg(g));
    emit txGainChanged(g);
}

void SettingsForm::setGain(qreal gain)
{
    if (gotGainSet) return;
    gotGainSet = true;
    qreal g = gain;
    int coarse = (g-1) * 5000;
    int fine = (g-1-coarse/5000.0) * 100000;
    ui->gainCoarseSlider->setValue(coarse);
    ui->gainFineSlider->setValue(fine);
}

void SettingsForm::setToneSync(bool b)
{
    ui->keyToneCheckBox->setChecked(b);
    if (b) {
        ui->keyToneSpinBox->setEnabled(false);
        ui->keyToneSpinBox->setValue(ui->rxToneSpinBox->value());
    } else {
        ui->keyToneSpinBox->setEnabled(true);
    }
}

void SettingsForm::setRxTone(int hz)
{
    ui->rxToneSpinBox->setValue(hz);
    if (ui->keyToneCheckBox->isChecked()) {
        ui->keyToneSpinBox->setValue(hz);
    }
}
