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

#ifndef AUDIO_OSX_H
#define AUDIO_OSX_H

#include "audio.h"
#include <array>
#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>

class Audio : public AudioBase
{
    Q_OBJECT
public:
    explicit Audio(class Radio *radio = 0);
    ~Audio();
    QString error;

private:
    static constexpr qreal OUTPUT_INTERVAL = 0.002;
    static constexpr qreal BUFLEVEL_ALPHA = 0.0005;

    virtual int mutePaddingFrames() {
        return 0.015 * 96000;
    }
    virtual qreal transmitPaddingSecs() {
        return OUTPUT_INTERVAL * 3;
    }

    AudioComponentInstance speakerInstance;
    AudioComponentInstance peaberryInstance;
    AudioBufferList captureBufferList;
    QVector<Float32> captureBufData;

    void findDevices(AudioDeviceID &speakerID, AudioDeviceID &peaberryID);
    QString convCFStringToQString(CFStringRef str);
    UInt32 createInstance(AudioComponent comp,
                          AudioDeviceID devID,
                          AudioComponentInstance *instance,
                          AudioStreamBasicDescription *format,
                          bool isRadio);
    void configureSpeaker(AudioComponent comp, AudioDeviceID speakerID);
    void configurePeabery(AudioComponent comp, AudioDeviceID peaberryID);

    static OSStatus speakerCallback(void* inRefCon,
                                    AudioUnitRenderActionFlags* ioActionFlags,
                                    const AudioTimeStamp* inTimeStamp,
                                    UInt32 inBusNumber,
                                    UInt32 inNumberFrames,
                                    AudioBufferList* ioData);

    static OSStatus transmitCallback(void* inRefCon,
                                     AudioUnitRenderActionFlags* ioActionFlags,
                                     const AudioTimeStamp* inTimeStamp,
                                     UInt32 inBusNumber,
                                     UInt32 inNumberFrames,
                                     AudioBufferList* ioData);

    static OSStatus receiveCallback(void* inRefCon,
                                    AudioUnitRenderActionFlags* ioActionFlags,
                                    const AudioTimeStamp* inTimeStamp,
                                    UInt32 inBusNumber,
                                    UInt32 inNumberFrames,
                                    AudioBufferList* ioData);

public slots:
    void start();
    void stop();
};

#endif // AUDIO_H
