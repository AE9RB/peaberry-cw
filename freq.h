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

#ifndef FREQ_H
#define FREQ_H

#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QVector>

class Freq : public QWidget
{
    Q_OBJECT
public:
    explicit Freq(const QString units = "", QWidget *parent = 0);
    void setRange(qint64 freq1, qint64 freq2);
    qint64 value();

signals:
    void valueChanged(qint64 value);

public slots:
    void setValue(qint64 value);
    void setFont(const QFont font);
    void setUnitsFont(const QFont font);

private slots:
    void processSignChange();
    void processDigitChange(bool direct);

private:
    QHBoxLayout *hbox;
    QVector<QLabel*> separators;
    QVector<class FreqDigit*> digits;
    class FreqSign *sign;
    QHBoxLayout *units;
    qint64 min;
    qint64 max;
};


class FreqDigit : public QLabel
{
    Q_OBJECT
public:
    explicit FreqDigit(QWidget * parent = 0, Qt::WindowFlags f = 0);
    ~FreqDigit();
    FreqDigit *prev;
    FreqDigit *next;
    int value();
    bool setValue(int value);
    void cancelChange();

signals:
    void valueChanged(bool keyboard);

protected:
    bool event(QEvent * e);
    void enterEvent(QEvent * event);
    void leaveEvent(QEvent * event);
    void keyPressEvent(QKeyEvent * event);
    void focusInEvent(QFocusEvent * event);
    void focusOutEvent(QFocusEvent * event);
    void mousePressEvent(QMouseEvent * event);
    void wheelEvent(QWheelEvent * event);

private:
    void adjust(int dir, bool emit_changed = true);
    static FreqDigit *editing;
    int wheelPos;
    int val;
    int displayedVal;
};


class FreqSign : public QLabel
{
    Q_OBJECT
    int value_;
public:
    explicit FreqSign();
    ~FreqSign() {}
    int value() {
        return value_;
    }

protected:
    void mousePressEvent(QMouseEvent * event);
    void wheelEvent(QWheelEvent * event);

signals:
    void valueChanged();

public slots:
    void setValue(int value);

};

#endif // FREQ_H
