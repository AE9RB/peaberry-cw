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

#include "mainwindow.h"
#include "spectrumplot.h"
#include "freq.h"
#include "radio.h"

#include <QLabel>
#include <QPushButton>
#include <QGridLayout>
#include <QSlider>
#include <QComboBox>
#include <QCheckBox>
#include <QTimer>
#include <QDebug>

MainWindow::MainWindow(Radio *radio)
{
    setWindowTitle(QStringLiteral("Peaberry CW"));

    auto spectrumplot = new SpectrumPlot(this);

    tuner = new Freq(" Hz");
    QFont font(tuner->font());
    font.setPointSize(font.pointSize() * 2.5);
    tuner->setFont(font);
    font.setPointSize(font.pointSize() * 0.7);
    tuner->setUnitsFont(font);

    offset = new Freq;
    offset->setRange(-12000, 12000);
    offset->setValue(0);
    offset->setEnabled(false);
    font = offset->font();
    font.setPointSize(font.pointSize() * 2.0);
    offset->setFont(font);

    // Controls

    smeter = new QLabel(QStringLiteral(""));
    auto zoom = new QCheckBox(QStringLiteral("Zoom"));
    auto zerobeat = new QPushButton(QStringLiteral("Zero Beat"));
    zerobeat->setEnabled(false);
    auto settingsButton = new QPushButton(QStringLiteral("Settings"));
    powerSlider = new QSlider(Qt::Horizontal);
    powerInfo = new QLabel;
    keyerSlider = new QSlider(Qt::Horizontal);
    keyerInfo = new QLabel;
    filter = new QComboBox();
    agc = new QComboBox();
    gain = new QSlider(Qt::Vertical);
    xitrit = new QComboBox();

    // Control options

    powerSlider->setRange(0, 100);
    keyerSlider->setRange(4, 40);

    filter->addItem(QStringLiteral("1000 Hz"), 1000);
    filter->addItem(QStringLiteral("500 Hz"), 500);
    filter->addItem(QStringLiteral("250 Hz"), 250);
    filter->addItem(QStringLiteral("100 Hz"), 100);
    filter->addItem(QStringLiteral("50 Hz"), 50);

    agc->addItem(QStringLiteral("Fast"), 0.250);
    agc->addItem(QStringLiteral("Medium"), 0.500);
    agc->addItem(QStringLiteral("Slow"), 0.900);
    agc->addItem(QStringLiteral("Long"), 1.500);

    xitrit->addItem(QStringLiteral("Off"), 0);
    xitrit->addItem(QStringLiteral("XIT"), 1);
    xitrit->addItem(QStringLiteral("RIT"), 2);

    // Local UI signals

    connect(powerSlider, &QSlider::valueChanged, this, &MainWindow::powerHandler);
    connect(keyerSlider, &QSlider::valueChanged, this, &MainWindow::keyerHandler);
    connect(agc, SIGNAL(currentIndexChanged(int)), this, SLOT(agcHandler(int)));
    connect(filter, SIGNAL(currentIndexChanged(int)), this, SLOT(filterHandler(int)));
    connect(xitrit, SIGNAL(currentIndexChanged(int)), this, SLOT(xitritHandler(int)));

    // Layout

    auto scopeHBox = new QHBoxLayout();
    scopeHBox->addWidget(zoom);
    scopeHBox->addSpacerItem(
        new QSpacerItem(0, 0, QSizePolicy::MinimumExpanding, QSizePolicy::Fixed)
    );
    scopeHBox->addWidget(smeter);

    auto freqLayout = new QGridLayout();
    xitrit->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    freqLayout->addWidget(tuner, 0, 0, 1, 2);
    freqLayout->addWidget(xitrit, 1, 0, 1, 1, Qt::AlignVCenter);
    freqLayout->addWidget(offset, 1, 1, 1, 1, Qt::AlignVCenter);

    auto control_row1 = new QGridLayout();
    control_row1->addLayout(freqLayout, 0, 0, 2, 1, Qt::AlignCenter);
    control_row1->setColumnMinimumWidth(1, 15);
    control_row1->addLayout(scopeHBox, 0, 2, 1, 2);
    control_row1->addWidget(zerobeat, 1, 2, 1, 1);
    control_row1->addWidget(settingsButton, 1, 3, 1, 1);

    auto control_row2 = new QGridLayout();
    control_row2->addWidget(new QLabel(QStringLiteral("Power")), 0, 0);
    control_row2->addWidget(new QLabel(QStringLiteral("Keyer")), 1, 0);
    control_row2->addWidget(powerSlider, 0, 1);
    control_row2->addWidget(keyerSlider, 1, 1);
    control_row2->addWidget(powerInfo, 0, 2);
    control_row2->addWidget(keyerInfo, 1, 2);
    control_row2->setColumnMinimumWidth(3, 15);
    control_row2->addWidget(new QLabel(QStringLiteral("Filter")), 0, 4);
    control_row2->addWidget(filter, 0, 5);
    control_row2->addWidget(new QLabel(QStringLiteral("AGC")), 1, 4);
    control_row2->addWidget(agc, 1, 5);

    auto controls_vbox = new QVBoxLayout;
    controls_vbox->addLayout(control_row1);
    controls_vbox->addLayout(control_row2);

    auto central_hbox = new QHBoxLayout;
    central_hbox->setContentsMargins(11, 0, 11, 11);
    central_hbox->addSpacerItem(
        new QSpacerItem(0, 0, QSizePolicy::MinimumExpanding, QSizePolicy::Fixed)
    );
    central_hbox->addLayout(controls_vbox);
    gain->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    central_hbox->addWidget(gain);
    central_hbox->addSpacerItem(
        new QSpacerItem(0, 0, QSizePolicy::MinimumExpanding, QSizePolicy::Fixed)
    );

    auto central_vbox = new QVBoxLayout;
    central_vbox->setMargin(0);
    setLayout(central_vbox);
    central_vbox->addWidget(spectrumplot);
    central_vbox->addLayout(central_hbox);

    // Compute spacing for info areas

    powerSlider->setValue(powerSlider->maximum());
    powerSlider->setValue(powerSlider->minimum());
    keyerSlider->setValue(keyerSlider->maximum());
    control_row2->setColumnMinimumWidth(2,
                                        std::max(powerInfo->minimumSizeHint().width(),
                                                keyerInfo->minimumSizeHint().width())
                                       );
    setSmeter(27); // S9 + 100
    smeter->setMinimumWidth(smeter->minimumSizeHint().width());

    // Connect to radio
    connect(radio, SIGNAL(dbOffsetChanged(qreal)), spectrumplot, SLOT(setDbOffset(qreal)));
    connect(radio, SIGNAL(spectrumViewUpdate(QVector<qreal>*)),
            spectrumplot, SLOT(setData(QVector<qreal>*)));

    connect(radio, SIGNAL(colorsChanged(int)), spectrumplot, SLOT(setTheme(int)));

    connect(this, SIGNAL(keyerValueChanged(int)), radio, SLOT(setSpeed(int)));
    connect(radio, SIGNAL(speedChanged(int)), keyerSlider, SLOT(setValue(int)));

    connect(zoom, SIGNAL(toggled(bool)), radio, SLOT(setFftZoom(bool)));
    connect(radio, SIGNAL(fftZoomChanged(bool)), zoom, SLOT(setChecked(bool)));
    connect(radio, SIGNAL(fftZoomChanged(bool)), spectrumplot, SLOT(setZoom(bool)));

    connect(radio, SIGNAL(gainChanged(int)), gain, SLOT(setValue(int)));
    connect(gain, SIGNAL(valueChanged(int)), radio, SLOT(setGain(int)));

    connect(radio, SIGNAL(agcSpeedChanged(qreal)), this, SLOT(setAgcValue(qreal)));
    connect(this, SIGNAL(agcValueChanged(qreal)), radio, SLOT(setAgcSpeed(qreal)));

    connect(radio, SIGNAL(filterChanged(int)), this, SLOT(setFilterValue(int)));
    connect(this, SIGNAL(filterValueChanged(int)), radio, SLOT(setFilter(int)));
    connect(this, SIGNAL(filterValueChanged(int)), spectrumplot, SLOT(setFilter(int)));

    connect(settingsButton, SIGNAL(clicked()), this, SIGNAL(showSettings()));

    connect(radio, SIGNAL(smeterUpdate(qreal)), this, SLOT(setSmeter(qreal)));

    connect(spectrumplot, SIGNAL(freqAdjusted(qreal)), this, SLOT(freqAdjusted(qreal)));
    connect(spectrumplot, SIGNAL(offsetAdjusted(qreal)), this, SLOT(offsetAdjusted(qreal)));

    connect(this, SIGNAL(powerValueChanged(int)), radio, SLOT(setPower(int)));
    connect(radio, SIGNAL(powerChanged(int)), powerSlider, SLOT(setValue(int)));

    connect(radio, SIGNAL(freqChanged(qint64)), tuner, SLOT(setValue(qint64)));
    connect(tuner, SIGNAL(valueChanged(qint64)), radio, SLOT(setFreq(qint64)));

    connect(radio, SIGNAL(xitChanged(qint64)), this, SLOT(setXit(qint64)));
    connect(this, SIGNAL(xitValueChanged(qint64)), radio, SLOT(setXit(qint64)));

    connect(radio, SIGNAL(ritChanged(qint64)), this, SLOT(setRit(qint64)));
    connect(this, SIGNAL(ritValueChanged(qint64)), radio, SLOT(setRit(qint64)));

    connect(radio, SIGNAL(ritChanged(qint64)), spectrumplot, SLOT(setRit(qint64)));
    connect(radio, SIGNAL(xitChanged(qint64)), spectrumplot, SLOT(setXit(qint64)));
    connect(xitrit, SIGNAL(currentIndexChanged(int)), spectrumplot, SLOT(setXitRitEnabled(int)));

    connect(offset, SIGNAL(valueChanged(qint64)), this, SLOT(offsetHandler(qint64)));

}

void MainWindow::setKeyerValue(int wpm)
{
    keyerSlider->setValue(wpm);
}

void MainWindow::setAgcValue(qreal v)
{
    int index = agc->findData(v);
    if (index < 0) index = 1;
    agc->setCurrentIndex(index);
    emit agcValueChanged(agc->currentData().toReal());
}

void MainWindow::setFilterValue(int v)
{
    int index = filter->findData(v);
    if (index < 0) index = 1;
    filter->setCurrentIndex(index);
    emit filterValueChanged(filter->currentData().toInt());
}

void MainWindow::setSmeter(qreal v)
{
    //TODO this is all temporary junk until
    // a proper S-meter is created
    static qreal stickyValue = -999;
    static int stickyCountdown = 0;
    if (stickyCountdown && v < stickyValue) {
        --stickyCountdown;
        return;
    }
    stickyValue = v;
    stickyCountdown = 30; // 1 secs
    QString s("S");
    if (v >= -63) {
        s.append("9+");
        v += 73;
        unsigned int vv = v / 10;
        vv *= 10;
        s.append(QString::number(vv));
    } else {
        v += 127;
        if (v<0) v = 0;
        unsigned int vv = v / 6;
        if (vv > 9) vv = 9;
        s.append(QString::number(vv));
    }
    smeter->setText(s);
}

void MainWindow::powerHandler(int val)
{
    if (val <= powerSlider->minimum()) {
        powerInfo->setText(QStringLiteral("TX Off"));
    } else {
        QString s;
        s.setNum(val);
        s.append(QStringLiteral("%"));
        powerInfo->setText(s);
    }
    emit(powerValueChanged(val));
}

void MainWindow::keyerHandler(int val)
{
    if (val <= keyerSlider->minimum()) {
        keyerInfo->setText(QStringLiteral("Off"));
        emit(keyerValueChanged(0));
    } else {
        QString s;
        s.setNum(val);
        s.append(QStringLiteral(" WPM"));
        keyerInfo->setText(s);
        emit(keyerValueChanged(val));
    }
}

void MainWindow::agcHandler(int)
{
    qreal agcval = agc->currentData().toReal();
    emit agcValueChanged(agcval);
}

void MainWindow::filterHandler(int)
{
    int filterval = filter->currentData().toInt();
    emit filterValueChanged(filterval);
}

void MainWindow::freqAdjusted(qreal delta)
{
    qint64 newf;
    switch(xitrit->currentData().toInt()) {
    case 0:
    case 1: // xit
        newf = tuner->value() + delta;
        newf = round(newf / 10.0) * 10;
        tuner->setValue(newf);
        break;
    case 2: // rit
        delta += offset->value();
        delta = round(delta / 10.0) * 10;
        offset->setValue(delta);
        break;
    }
}

void MainWindow::offsetAdjusted(qreal delta)
{
    qint64 newf;
    switch(xitrit->currentData().toInt()) {
    case 0:
        newf = tuner->value() + delta;
        newf = round(newf / 10.0) * 10;
        tuner->setValue(newf);
        break;
    case 1: // xit
        delta = round(delta / 10.0) * 10;
        offset->setValue(delta);
        break;
    case 2: // rit
        newf = tuner->value() + offset->value() + delta;
        newf = round(newf / 10.0) * 10;
        tuner->setValue(newf);
        delta = round(delta / 10.0) * 10;
        offset->setValue(-delta);
        break;
    }
}

void MainWindow::xitritHandler(int)
{
    switch(xitrit->currentData().toInt()) {
    case 0:
        offset->setEnabled(false);
        emit xitValueChanged(0);
        emit ritValueChanged(0);
        offset->setValue(0);
        break;
    case 1: // xit
        offset->setEnabled(true);
        emit xitValueChanged(offset->value());
        emit ritValueChanged(0);
        break;
    case 2: // rit
        offset->setEnabled(true);
        emit xitValueChanged(0);
        emit ritValueChanged(offset->value());
        break;
    }
}

void MainWindow::offsetHandler(qint64 f)
{
    switch(xitrit->currentData().toInt()) {
    case 1: // xit
        emit xitValueChanged(f);
        break;
    case 2: // rit
        emit ritValueChanged(f);
        break;
    }
}

void MainWindow::setXit(qint64 f)
{
    if (f) {
        offset->setValue(f);
        xitrit->setCurrentIndex(1);
    }
}

void MainWindow::setRit(qint64 f)
{
    if (f) {
        offset->setValue(f);
        xitrit->setCurrentIndex(2);
    }
}
