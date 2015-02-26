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

#ifndef SPECTRUMPLOT_H
#define SPECTRUMPLOT_H

#include <QtCore>
#include <QGLWidget>

class SpectrumPlot : public QGLWidget
{
    Q_OBJECT
public:
    explicit SpectrumPlot(QWidget *parent);
    ~SpectrumPlot();
    enum class Theme : int {
        Winrad=0,
        Linrad,
        Grayscale,
        Spectran,
        SpectranExtended,
        Horne
    };

signals:
    void freqAdjusted(qreal delta);
    void offsetAdjusted(qreal delta);

public slots:
    void setTheme(int v);
    void setZoom(bool z);
    void setMinDb(int v);
    void setRangeDb(int v);
    void setData(QVector<qreal> * vals);
    void setFilter(int hz);
    void setDbOffset(qreal db);
    void setXit(qint64 f);
    void setRit(qint64 f);
    void setXitRitEnabled(int v);
    virtual QSize sizeHint() const;

protected:
    void paintEvent(QPaintEvent *event);
    void resizeEvent(QResizeEvent * event);
    void mousePressEvent(QMouseEvent * event);

private:
    QGradientStops intensity;
    QColor background;
    QColor plotLine;
    QColor dbLine;
    QColor dbText;
    int rangeDb;
    int minDb;
    int filter;
    bool zoom;
    bool needsRecalc;
    qint64 xit;
    qint64 rit;
    bool xitrit;

    QPixmap *labels;
    int stepSize;
    int firstLine;
    QVector<QPointF> polyData;

};

#endif // SPECTRUMPLOT_H
