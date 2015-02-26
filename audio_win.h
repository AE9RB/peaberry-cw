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

#ifndef AUDIO_WIN_H
#define AUDIO_WIN_H

#include "audio.h"
#include <array>

#include <windows.h>
#include <AudioClient.h>
#include <mmdeviceapi.h>

extern const IID IID_IAudioClient;
extern const IID IID_IAudioRenderClient;

class Audio : public AudioBase
{
    friend class AudioWorkerThread;
    Q_OBJECT
public:
    explicit Audio(class Radio *radio = 0);
    ~Audio();
    QString error;

private:
    static constexpr qreal BUFLEVEL_ALPHA = 0.0005;

    virtual int mutePaddingFrames();
    virtual qreal transmitPaddingSecs();

    HRESULT findSpeakerWavex(WAVEFORMATEXTENSIBLE *wavex);
    void findPeaberry(IMMDeviceEnumerator *pEnumerator,
                      bool isTransmit,
                      IMMDevice **device,
                      IAudioClient **audioClient,
                      WAVEFORMATEXTENSIBLE *wavex,
                      const QString &errstr);
    void setPeaberryVolume(IMMDevice *device,
                           const QString &errstr);
    void initializeAudioClient(IMMDevice *device,
                               WAVEFORMATEX* wfx,
                               IAudioClient **audioClient,
                               bool *callbackMode,
                               UINT32 *bufferSize,
                               const QString &errstr);


public slots:
    void start();
    void stop();

private:
    IMMDevice *speakerDevice;
    IAudioClient *speakerAudioClient;
    IAudioRenderClient *speakerRenderClient;
    HANDLE speakerEvent;
    UINT32 speakerBufferFrames;
    bool speakerCallbackMode;
    quint8 speakerFormat;

    IMMDevice *receiveDevice;
    IAudioClient *receiveAudioClient;
    IAudioCaptureClient *receiveCaptureClient;
    HANDLE receiveEvent;
    UINT32 receiveBufferFrames;
    bool receiveCallbackMode;

    IMMDevice *transmitDevice;
    IAudioClient *transmitAudioClient;
    IAudioRenderClient *transmitRenderClient;
    HANDLE transmitEvent;
    UINT32 transmitBufferFrames;
    bool transmitCallbackMode;

    class AudioWorkerThread *worker;
};


class AudioWorkerThread : public QThread
{
    Q_OBJECT
public:
    AudioWorkerThread(Audio *audio) :
        QThread(audio),
        audio(audio) {}
    ~AudioWorkerThread() {}
    bool running;
private:
    Audio *audio;
    QElapsedTimer etimer;
    qint64 emark;
    void run();
    void doSpeaker(int frames);
    template <class T> void doSpeakerGeneric(int frames);
    void doReceive();
    void doTransmit(int frames);
};

#endif // AUDIO_WIN_H
