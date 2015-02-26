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

#include <QApplication>
#include <QMessageBox>
#include "radio.h"
#include "mainwindow.h"
#include "settingsform.h"
#include "dsp.h"

int main(int argc, char *argv[])
{
    qRegisterMetaType<REAL>("TYPEREAL");
    qRegisterMetaType<COMPLEX>("COMPLEX");

    QApplication a(argc, argv);

    QLocale::setDefault(QLocale::system());

    Radio radio;
    if (!radio.error.isEmpty()) {
        QMessageBox msgBox;
        msgBox.setWindowTitle("Peaberry CW Error");
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setText(radio.error);
        msgBox.exec();
        return EXIT_FAILURE;
    }

    MainWindow w(&radio);
    SettingsForm s(&radio);

    QObject::connect(&w, SIGNAL(showSettings()), &s, SLOT(show()));

    radio.load();

    w.show();
    return a.exec();
}
