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

#include "spectrumplot.h"
#include <QApplication>
#include <QMouseEvent>
#include <QDebug>
#include <QPainter>

SpectrumPlot::SpectrumPlot(QWidget *parent) :
    QGLWidget(parent)
{
    minDb = -100;
    rangeDb = 110;
    filter = 500;
    labels = new QPixmap(0,0);
    stepSize = 1;
    firstLine = minDb;
    xitrit = false;
    xit = rit = 0;
    setTheme((int)Theme::Winrad);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
}

SpectrumPlot::~SpectrumPlot()
{
    delete labels;
}

void SpectrumPlot::setData(QVector<qreal> *vals)
{
    if (vals->size() + 4 != polyData.size()) {
        polyData.resize(vals->size() + 4);
        for (int x = 0, xx = vals->size(); x < xx; ++x) {
            polyData[x].setX(x);
        }
    }
    int x = 0;
    for (int xx = vals->size(); x < xx; ++x) {
        polyData[x].setY(-(*vals)[x]);
    }
    polyData[x] = QPointF(x+1,polyData[x-1].y());
    polyData[x+1] = QPointF(x+1,1000);
    polyData[x+2] = QPointF(-2,1000);
    polyData[x+3] = QPointF(-2,polyData[0].y());
    update();
}

void SpectrumPlot::setFilter(int hz)
{
    // Add fudge for filter rolloff
    filter = hz + 75;
    update();
}

void SpectrumPlot::setDbOffset(qreal db)
{
    setMinDb(-130 + db);
}

void SpectrumPlot::setXit(qint64 f)
{
    xit = f;
}

void SpectrumPlot::setRit(qint64 f)
{
    rit = f;
}

void SpectrumPlot::setXitRitEnabled(int v)
{
    xitrit = v;
}

void SpectrumPlot::paintEvent(QPaintEvent *event)
{
    if (needsRecalc) {
        needsRecalc = false;
        delete labels;
        qreal yscale = (qreal)rect().height() / rangeDb;
        auto ft = font();
        ft.setPixelSize(11);
        QFontMetrics fontMetrics(ft);
        auto lineWidth = fontMetrics.size(Qt::TextSingleLine, "-000").width();
        labels = new QPixmap(lineWidth, height());
        labels->fill(Qt::transparent);
        if (height()) {
            QPainter p(labels);
            p.setFont(ft);
            p.setPen(dbText);
            int fh = fontMetrics.height();
            stepSize = rangeDb;
            if (yscale > fh) stepSize = 1;
            else if (yscale * 5 > fh) stepSize = 5;
            else if (yscale * 10 > fh) stepSize = 10;
            else if (yscale * 25 > fh) stepSize = 25;
            else if (yscale * 50 > fh) stepSize = 50;
            qreal fontOffset = fontMetrics.tightBoundingRect("0").height() * 0.5;
            qreal y = -(minDb + rangeDb) - stepSize;
            while ((int)y%stepSize) ++y;
            firstLine = y;
            while (y < -minDb + stepSize) {
                qreal yy = (y + minDb + rangeDb) * yscale;
                p.drawText(0, yy + fontOffset, QString::number(-y));
                y += stepSize;
            }
        }
    }
    int topStart = minDb + rangeDb;
    makeCurrent();
    QPainter p(this);
    p.fillRect(rect(), background);
    qreal xscale = (qreal)rect().width() / (polyData.size()-4);
    qreal yscale = (qreal)rect().height() / rangeDb;
    qreal xoffset = 0;
    if (zoom && xscale < 1.0) {
        xoffset = -((polyData.size()-4) - (qreal)rect().width())/2;
        xscale = 1.0;
    }
    p.scale(xscale, yscale);
    p.translate(xoffset, topStart);
    QPen polypen(plotLine);
    polypen.setWidth(0);
    QLinearGradient grad(0, -topStart, 0, -minDb);
    grad.setStops(intensity);
    p.setBrush(grad);
    p.setPen(polypen);
    p.drawPolygon(polyData.data(), polyData.size());
    p.setWorldTransform(QTransform()); // reset
    p.setPen(dbLine);
    for (int y = firstLine; y < -minDb + stepSize; y += stepSize) {
        int yy = (y + topStart) * yscale;
        p.drawLine(0, yy, width(), yy);
    }
    p.drawPixmap(2, 0, *labels);

    const qreal binSize = 96000.0 / 8192;

    p.scale(xscale, 1.0);
    p.translate(xoffset, 0);
    QColor filterColor(Qt::yellow);
    filterColor.setAlpha(33);
    QRectF filterRect(rect());
    qreal filterWidth = filter / binSize;
    filterRect.setX(polyData.size() / 2.0 - filterWidth / 2.0 - 1);
    filterRect.setWidth(filterWidth);
    p.fillRect(filterRect, filterColor);

    if (xitrit) {
        qreal transmitX = polyData.size() / 2.0 - 2.0 + (xit - rit) / binSize;
        QColor transmitColor(Qt::red);
        transmitColor.setAlpha(75);
        QRectF transmitRect(rect());
        transmitRect.setX(transmitX);
        transmitRect.setWidth(1/xscale * 2);
        p.fillRect(transmitRect, transmitColor);
    }

    event->accept();
    // eat event
}

void SpectrumPlot::resizeEvent(QResizeEvent *event)
{
    if (height() != labels->height()) {
        needsRecalc = true;
        update();
    }
    QGLWidget::resizeEvent(event);
}

void SpectrumPlot::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton) {
        qreal xscale = (qreal)rect().width() / (polyData.size()-4);
        qreal xoffset = 0;
        if (zoom && xscale < 1.0) {
            xoffset = -((polyData.size()-4) - (qreal)rect().width())/2;
            xscale = 1.0;
        }
        const qreal binSize = 96000.0 / 8192;
        qreal adj = binSize * ((event->pos().x()-1) / xscale - xoffset) - 15000;
        if (event->button() == Qt::LeftButton) emit freqAdjusted(adj);
        else emit offsetAdjusted(adj);
        event->accept();
    }
    QGLWidget::mousePressEvent(event);
}

QSize SpectrumPlot::sizeHint() const
{
    return QSize(300,150);
}

void SpectrumPlot::setTheme(int v)
{
    switch((Theme)v) {
    case Theme::Linrad:
        intensity.clear();
        intensity.push_back(QPair<double, QColor>(0.0, QColor("#ff0000")));
        intensity.push_back(QPair<double, QColor>(0.5, QColor("#008020")));
        intensity.push_back(QPair<double, QColor>(1.0, QColor("#000030")));
        background.setNamedColor("#000000");
        plotLine.setNamedColor("#808080");
        dbLine.setNamedColor("#292761");
        dbText.setNamedColor("#DDDDDD");
        break;
    case Theme::Grayscale:
        intensity.clear();
        intensity.push_back(QPair<double, QColor>(0.0, QColor("#f0f0f0")));
        intensity.push_back(QPair<double, QColor>(1.0, QColor("#000000")));
        background.setNamedColor("#000000");
        plotLine.setNamedColor("#808080");
        dbLine.setNamedColor("#282828");
        dbText.setNamedColor("#DDDDDD");
        break;
    case Theme::Spectran:
        intensity.clear();
        intensity.push_back(QPair<double, QColor>(0.0, QColor("#f0f0f0")));
        intensity.push_back(QPair<double, QColor>(0.5, QColor("#5080d0")));
        intensity.push_back(QPair<double, QColor>(1.0, QColor("#000030")));
        background.setNamedColor("#000000");
        plotLine.setNamedColor("#808080");
        dbLine.setNamedColor("#292761");
        dbText.setNamedColor("#DDDDDD");
        break;
    case Theme::SpectranExtended:
        intensity.clear();
        intensity.push_back(QPair<double, QColor>(0.0, QColor("#ffff48")));
        intensity.push_back(QPair<double, QColor>(0.5, QColor("#A00080")));
        intensity.push_back(QPair<double, QColor>(1.0, QColor("#100080")));
        background.setNamedColor("#000000");
        plotLine.setNamedColor("#808080");
        dbLine.setNamedColor("#292761");
        dbText.setNamedColor("#DDDDDD");
        break;
    case Theme::Horne:
        intensity.clear();
        intensity.push_back(QPair<double, QColor>(0.0, QColor("#ffff48")));
        intensity.push_back(QPair<double, QColor>(0.5, QColor("#00ffff")));
        intensity.push_back(QPair<double, QColor>(1.0, QColor("#2600f8")));
        background.setNamedColor("#000000");
        plotLine.setNamedColor("#808080");
        dbLine.setNamedColor("#292761");
        dbText.setNamedColor("#DDDDDD");
        break;
    default: // (#0) Winrad
        intensity.clear();
        intensity.push_back(QPair<double, QColor>(0.0, QColor("#ff0000")));
        intensity.push_back(QPair<double, QColor>(0.5, QColor("#008000")));
        intensity.push_back(QPair<double, QColor>(1.0, QColor("#000080")));
        background.setNamedColor("#020131");
        plotLine.setNamedColor("#808080");
        dbLine.setNamedColor("#2a2561");
        dbText.setNamedColor("#DDDDDD");
        break;
    }
    needsRecalc = true;
    update();
}

void SpectrumPlot::setZoom(bool z)
{
    zoom = z;
    update();
}

void SpectrumPlot::setMinDb(int v)
{
    minDb = v;
    needsRecalc = true;
    update();
}

void SpectrumPlot::setRangeDb(int v)
{
    rangeDb = v;
    needsRecalc = true;
    update();
}
