VERSION = 0.0
VERSTR = '\\"$${VERSION}\\"'  # place quotes around the version string
DEFINES += VER=\"$${VERSTR}\" # create a VER macro

QT += core gui widgets opengl

QMAKE_CXXFLAGS += -std=c++11

macx {
    TARGET = "Peaberry CW"
    SOURCES += audio_osx.cpp
    HEADERS += audio_osx.h
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.7
    QMAKE_CXXFLAGS += -stdlib=libc++
    LIBS += -stdlib=libc++
    INCLUDEPATH += /opt/local/include/libusb-legacy
    LIBS += -L/opt/local/lib/libusb-legacy -lusb-legacy
    LIBS += -framework ApplicationServices -framework CoreAudio
    LIBS += -framework AudioUnit -framework AudioToolbox
}

win32 {
    TARGET = "PeaberryCW"
    SOURCES += audio_win.cpp
    HEADERS += audio_win.h
    HEADERS += libusb-win32/usb.h
    INCLUDEPATH += $$PWD/libusb-win32
    LIBS += -L$$PWD/libusb-win32 -lusb
    LIBS += -lole32 -lwinmm -lavrt
}

TEMPLATE = app

SOURCES += \
    main.cpp \
    radio.cpp \
    cat.cpp \
    keyer.cpp \
    mainwindow.cpp \
    freq.cpp \
    settingsform.cpp \
    spectrumplot.cpp \
    spectrum.cpp \
    agc.cpp \
    demod.cpp \
    audio.cpp

HEADERS  += \
    dsp.h \
    radio.h \
    cat.h \
    keyer.h \
    mainwindow.h \
    freq.h \
    fft.h \
    settingsform.h \
    spectrumplot.h \
    spectrum.h \
    agc.h \
    demod.h \
    audio.h

FORMS    += \
    settingsform.ui

RESOURCES +=
