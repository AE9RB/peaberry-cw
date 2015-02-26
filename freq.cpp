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

#include "freq.h"
#include <QKeyEvent>
#include <QApplication>
#include <QEvent>
#include <QLabel>

Freq::Freq(const QString units, QWidget *parent) :
    QWidget(parent)
{
    this->units = new QHBoxLayout();
    this->units->addWidget(new QLabel(units));
    this->units->setMargin(0);
    this->units->setAlignment(Qt::AlignBottom);

    sign = new FreqSign();

    hbox = new QHBoxLayout(this);
    hbox->layout()->setSpacing(0);
    hbox->layout()->setMargin(0);
    setRange(500000, 39999999);
    setValue(14070000);

    setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));

    connect(sign, SIGNAL(valueChanged()), this, SLOT(processSignChange()));
}

void Freq::setRange(qint64 freq1, qint64 freq2)
{
    if (freq1 < freq2) {
        min = freq1;
        max = freq2;
    }
    else {
        min = freq2;
        max = freq1;
    }

    int d = 0;
    for (qint64 m = max; m; m /= 10) d++;

    if (digits.size() != d) {
        foreach (QLabel *o, separators) delete o;
        separators.resize(0);
        foreach (FreqDigit *o, digits) delete o;
        digits.resize(0);
        hbox->removeItem(units);

        if (freq1 < 0) {
            hbox->addWidget(sign);
        }

        FreqDigit *prev = nullptr;
        for (int i = d-1; i >= 0; --i) {
            FreqDigit *digit = new FreqDigit();
            digit->setAlignment(Qt::AlignBottom);
            digit->prev = prev;
            digits.push_back(digit);
            hbox->addWidget(digit);
            if (prev) prev->next = digit;
            prev = digit;
            if (!(i%3) && i) {
                QLabel *label = new QLabel(QLocale().groupSeparator(), this);
                separators.push_back(label);
                hbox->addWidget(label);
            }
            connect(digit, SIGNAL(valueChanged(bool)), this, SLOT(processDigitChange(bool)));
        }
        hbox->addLayout(units);
    }

    qint64 f = value();
    if (f > max) setValue(max);
    else if (f < min) setValue(min);
}

void Freq::setFont(const QFont font)
{
    QWidget::setFont(font);
    setUnitsFont(units->itemAt(0)->widget()->font());
    sign->setValue(-sign->value());
    sign->setValue(-sign->value());
}

void Freq::setUnitsFont(const QFont font)
{
    units->itemAt(0)->widget()->setFont(font);
    int digitDescent = QFontMetrics(this->font()).descent();
    int unitsDescent = QFontMetrics(units->itemAt(0)->widget()->font()).descent();
    units->setContentsMargins(0, 0, 0, digitDescent - unitsDescent);
}

void Freq::processSignChange()
{
    emit valueChanged(value());
}

void Freq::setValue(qint64 freq)
{
    if (freq < min) freq = min;
    if (freq > max) freq = max;
    auto f = abs(freq);
    for (auto it = digits.end(), end = digits.begin(); it != end;) {
        --it;
        (*it)->setValue(f%10);
        f /= 10;
    }
    if (freq < 0) {
        sign->setValue(-1);
    } else {
        sign->setValue(1);
    }
    emit valueChanged(freq);
}

qint64 Freq::value()
{
    qint64 freq = 0;
    foreach (FreqDigit *o, digits) {
        freq *= 10;
        freq += o->value();
    }
    return freq * sign->value();
}

void Freq::processDigitChange(bool direct)
{
    qint64 f = value();
    bool didchange = false;
    if (direct) {
        if (f > max) {
            setValue(max);
            f = max;
            didchange = true;
        }
        else if (f < min) {
            setValue(min);
            f = max;
            didchange = true;
        }
    } else {
        if (f > max || f < min)
            foreach (FreqDigit *o, digits) o->cancelChange();
    }
    foreach (FreqDigit *o, digits) didchange = o->setValue(o->value()) || didchange;
    if (didchange) {
        emit valueChanged(f);
    }
}

FreqDigit* FreqDigit::editing = nullptr;

FreqDigit::FreqDigit(QWidget *parent, Qt::WindowFlags f) :
    QLabel("0", parent, f),
    prev(nullptr),
    next(nullptr),
    wheelPos(0),
    val(0),
    displayedVal(0)
{
    setTextInteractionFlags(Qt::TextSelectableByMouse);
    setAcceptDrops(false);
    setContextMenuPolicy(Qt::CustomContextMenu);
}

FreqDigit::~FreqDigit()
{
    if (editing == this) editing = nullptr;
}

int FreqDigit::value()
{
    return val;
}

bool FreqDigit::setValue(int value)
{
    val = value;
    if (val != displayedVal) {
        displayedVal = val;
        setText(QString::number(val));
        if (hasFocus()) setSelection(0,1);
        return true;
    }
    return false;
}

void FreqDigit::cancelChange()
{
    val = text().toInt();
}

void FreqDigit::adjust(int dir, bool emit_changed)
{
    bool ok = false;
    FreqDigit *d = this;
    if (dir > 0) {
        while (!ok && d) {
            if (d->value() != 9) ok = true;
            d = d->prev;
        }
        if (ok) ++val;
        if (val > 9) {
            val = 0;
            if (prev) prev->adjust(dir, false);
        }
    } else {
        while (!ok && d) {
            if (d->value() != 0) ok = true;
            d = d->prev;
        }
        if (ok) --val;
        if (val < 0) {
            val = 9;
            if (prev) prev->adjust(dir, false);
        }
    }
    if (ok && emit_changed) emit valueChanged(false);
}

bool FreqDigit::event(QEvent *e)
{
    if (e->type() == QEvent::KeyPress) {
        QKeyEvent *event = reinterpret_cast<QKeyEvent*>(e);
        switch(event->key()) {
        case Qt::Key_Delete:
        case Qt::Key_Tab:
        case Qt::Key_Right:
            clearFocus();
            if (next) {
                next->setFocus();
                return true;
            }
            break;
        case Qt::Key_Backspace:
        case Qt::Key_Backtab:
        case Qt::Key_Left:
            clearFocus();
            if (prev) {
                prev->setFocus();
                return true;
            }
            break;
        }
    }
    return QLabel::event(e);
}

void FreqDigit::enterEvent(QEvent *event)
{
    wheelPos = 0;
    QLabel::enterEvent(event);
}

void FreqDigit::leaveEvent(QEvent *event)
{
    QLabel::leaveEvent(event);
}

void FreqDigit::keyPressEvent(QKeyEvent *event)
{
    switch(event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        clearFocus();
        return;
    case Qt::Key_Plus:
    case Qt::Key_Up:
        adjust(1);
        return;
    case Qt::Key_Minus:
    case Qt::Key_Down:
        adjust(-1);
        return;
    }

    int value = event->key() - Qt::Key_0;
    if (value >= 0 && value <= 9) {
        if (val != value) {
            val = value;
            emit valueChanged(true);
        }
        clearFocus();
        if (next) next->setFocus();
        return;
    }

    QLabel::keyPressEvent(event);
}

void FreqDigit::focusInEvent(QFocusEvent *event)
{
    editing = this;
    setSelection(0,1);
    QLabel::focusInEvent(event);
}


void FreqDigit::focusOutEvent(QFocusEvent *event)
{
    editing = nullptr;
    QLabel::focusOutEvent(event);
}

void FreqDigit::mousePressEvent(QMouseEvent *event)
{
    Q_UNUSED(event)
    // eat event
}

void FreqDigit::wheelEvent(QWheelEvent *event)
{
    if (editing) editing->clearFocus();
    wheelPos += event->angleDelta().y();
    while (wheelPos >= 120) {
        adjust(1);
        wheelPos -= 120;
    }
    while (wheelPos <= -120) {
        adjust(-1);
        wheelPos += 120;
    }
    QLabel::wheelEvent(event);
}


FreqSign::FreqSign()
{
    setValue(1);
}

void FreqSign::mousePressEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    setValue(-value_);
}

void FreqSign::wheelEvent(QWheelEvent *event)
{
    Q_UNUSED(event);
    setValue(-value_);
}

void FreqSign::setValue(int value)
{
    if (value_ != value) {
        if (value < 0) setText("-");
        else setText("+");
        value_ = value;
        emit valueChanged();
    }
    setMinimumWidth(minimumSizeHint().width());
}
