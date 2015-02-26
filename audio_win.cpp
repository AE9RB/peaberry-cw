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

#include "audio_win.h"
#include "radio.h"
#include "avrt.h"
#include "endpointvolume.h"
#include <QSettings>

// Extra Windows constants
#define pcw_KSDATAFORMAT_SUBTYPE_PCM (GUID) {0x00000001,0x0000,0x0010,{0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71}}
#define pcw_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT (GUID) {0x00000003,0x0000,0x0010,{0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71}}
PROPERTYKEY PKEY_Device_FriendlyName = {{0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 14};
PROPERTYKEY PKEY_DeviceInterface_FriendlyName = {{0x026e516e, 0xb814, 0x414b, { 0x83, 0xcd, 0x85, 0x6d, 0x6f, 0xef, 0x48, 0x22}}, 2};
const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);
const IID IID_IAudioEndpointVolume = __uuidof(IAudioEndpointVolume);

// Error handling Microsoft-style.
#define SAFE_RELEASE(punk)  if ((punk) != NULL) { (punk)->Release(); (punk) = NULL; }
#define EXIT_ON_HR_ERROR(msg) if (FAILED(hr)) { error = msg; goto Exit; }
#define EXIT_ERROR(msg) {error = msg; goto Exit;}

// Long message about exclusive mode used in multiple places
static const char DOES_NOT_ALLOW_EXCLUSIVE[] =
    "device does not allow exclusive mode. This needs "
    "to be turned on in Control Panel > Sound > Properties > Advanced.";

// Many speaker output formats are supported with a generic
static const quint8 SPEAKER_UNKNOWN = 0;
static const quint8 SPEAKER_INT16_MONO = 1;
class AudioSpeakerInt16Mono {
    qint16 data;
public:
    inline AudioSpeakerInt16Mono& operator=(qreal v) {
        data = v * 32767;
        return *this;
    }
};
static const quint8 SPEAKER_INT16_STEREO = 2;
class AudioSpeakerInt16Stereo {
    qint16 left, right;
public:
    inline AudioSpeakerInt16Stereo& operator=(qreal v) {
        left = right = v * 32767;
        return *this;
    }
};
static const quint8 SPEAKER_FLOAT_MONO = 3;
class AudioSpeakerFloatMono {
    float data;
public:
    inline AudioSpeakerFloatMono& operator=(qreal v) {
        data = v;
        return *this;
    }
};
static const quint8 SPEAKER_FLOAT_STEREO = 4;
class AudioSpeakerFloatStereo {
    float left, right;
public:
    inline AudioSpeakerFloatStereo& operator=(qreal v) {
        left = right = v;
        return *this;
    }
};


Audio::Audio(Radio *radio) :
    AudioBase(radio),
    speakerDevice(NULL),
    speakerAudioClient(NULL),
    speakerRenderClient(NULL),
    speakerEvent(NULL),
    speakerFormat(SPEAKER_UNKNOWN),
    receiveDevice(NULL),
    receiveAudioClient(NULL),
    receiveCaptureClient(NULL),
    receiveEvent(NULL),
    transmitDevice(NULL),
    transmitAudioClient(NULL),
    transmitRenderClient(NULL),
    transmitEvent(NULL)
{
    HRESULT hr;
    IMMDeviceEnumerator *pEnumerator = NULL;
    WAVEFORMATEXTENSIBLE *wavex = NULL;
    WAVEFORMATEX *wfx = NULL;
    LPWSTR speakerID = NULL;
    LPWSTR transmitID = NULL;

    // Friendly error prefixes
    static const QString ERROR_SPEAKER = QString("Default sound playback ");
    static const QString ERROR_RECEIVE = QString("Peaberry Radio recording ");
    static const QString ERROR_TRANSMIT = QString("Peaberry Radio playback ");

    // Peabery Audio events never fire anywhere I tested
    receiveCallbackMode = false;
    transmitCallbackMode = false;
    // Allow override in settings for faulty sound drivers
    speakerCallbackMode = radio->settings()->value("eventSpeaker", true).toBool();
    // Ensure settings are saved for easy user editing
    radio->settings()->setValue("eventSpeaker", speakerCallbackMode);

    // The main Qt thread already has this initialized
    //hr = CoInitialize(NULL);
    //EXIT_ON_HR_ERROR("CoInitialize");

    // Operating systems without WASAPI fail here
    hr = CoCreateInstance(
             CLSID_MMDeviceEnumerator, NULL,
             CLSCTX_ALL, IID_IMMDeviceEnumerator,
             (void**)&pEnumerator);
    EXIT_ON_HR_ERROR("Incompatible Operating System.");

    wavex = (WAVEFORMATEXTENSIBLE*)CoTaskMemAlloc(sizeof(WAVEFORMATEXTENSIBLE));
    wfx = (WAVEFORMATEX*)wavex;
    if (wavex == NULL) EXIT_ERROR("CoTaskMemAlloc(WAVEFORMATEX)");

    findPeaberry(pEnumerator, false, &receiveDevice, &receiveAudioClient, wavex, ERROR_RECEIVE);
    if (!receiveDevice) goto Exit;
    setPeaberryVolume(receiveDevice, ERROR_RECEIVE);
    if (!error.isEmpty()) goto Exit;
    initializeAudioClient(receiveDevice, wfx, &receiveAudioClient, &receiveCallbackMode, &receiveBufferFrames, ERROR_RECEIVE);
    if (!error.isEmpty()) goto Exit;
    qDebug() << "receiveCallbackMode=" << receiveCallbackMode << "receiveBufferFrames=" << receiveBufferFrames;

    findPeaberry(pEnumerator, true, &transmitDevice, &transmitAudioClient, wavex, ERROR_TRANSMIT);
    if (!transmitDevice) goto Exit;
    setPeaberryVolume(transmitDevice, ERROR_TRANSMIT);
    if (!error.isEmpty()) goto Exit;
    initializeAudioClient(transmitDevice, wfx, &transmitAudioClient, &transmitCallbackMode, &transmitBufferFrames, ERROR_TRANSMIT);
    if (!error.isEmpty()) goto Exit;
    qDebug() << "transmitCallbackMode=" << transmitCallbackMode << "transmitBufferFrames=" << transmitBufferFrames;

    hr = pEnumerator->GetDefaultAudioEndpoint(
             eRender, eConsole, &speakerDevice);
    EXIT_ON_HR_ERROR(ERROR_SPEAKER + "device not found.");

    // Check for radio set as default output device before
    // speakerDevice->Activate gives user confusing message
    hr = speakerDevice->GetId(&speakerID);
    EXIT_ON_HR_ERROR(ERROR_SPEAKER + "IMMDevice::GetId");
    hr = transmitDevice->GetId(&transmitID);
    EXIT_ON_HR_ERROR(ERROR_TRANSMIT + "IMMDevice::GetId");
    if (QString((QChar*)speakerID) == QString((QChar*)transmitID)) {
        EXIT_ERROR("Peaberry Radio is currently configured as the default "
                   "sound playback device. You must select a different "
                   "device in Control Panel > Sound > Playback.");
    }

    hr = speakerDevice->Activate(
             IID_IAudioClient, CLSCTX_ALL,
             NULL, (void**)&speakerAudioClient);
    EXIT_ON_HR_ERROR(ERROR_SPEAKER + "IMMDevice::Activate")

    hr = findSpeakerWavex(wavex);
    if (hr==AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED) {
        EXIT_ERROR(ERROR_SPEAKER + DOES_NOT_ALLOW_EXCLUSIVE);
    }
    EXIT_ON_HR_ERROR(ERROR_SPEAKER + "formats not supported.");

    if (wfx->wFormatTag == WAVE_FORMAT_PCM ||
            (wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
             wavex->SubFormat == pcw_KSDATAFORMAT_SUBTYPE_PCM)) {
        if (wfx->nChannels == 1) speakerFormat = SPEAKER_INT16_MONO;
        else speakerFormat = SPEAKER_INT16_STEREO;
    }
    if (wfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
            (wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
             wavex->SubFormat == pcw_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
        if (wfx->nChannels == 1) speakerFormat = SPEAKER_FLOAT_MONO;
        else speakerFormat = SPEAKER_FLOAT_STEREO;
    }

    speakerSampleRate = wfx->nSamplesPerSec;

    initializeAudioClient(speakerDevice, wfx, &speakerAudioClient, &speakerCallbackMode, &speakerBufferFrames, ERROR_SPEAKER);
    if (!error.isEmpty()) goto Exit;

    qDebug() << "speakerCallbackMode=" << speakerCallbackMode
             << "speakerBufferFrames=" << speakerBufferFrames
             << "speakerSampleRate=" << speakerSampleRate
             << "speakerFormat=" << speakerFormat;

    speakerEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    receiveEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    transmitEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (speakerEvent == NULL || receiveEvent == NULL || transmitEvent == NULL) {
        error = "CreateEvent";
        goto Exit;
    }

    if (speakerCallbackMode) {
        hr = speakerAudioClient->SetEventHandle(speakerEvent);
        EXIT_ON_HR_ERROR(ERROR_SPEAKER + "IAudioClient::SetEventHandle");
    }
    if (receiveCallbackMode) {
        hr = receiveAudioClient->SetEventHandle(speakerEvent);
        EXIT_ON_HR_ERROR(ERROR_RECEIVE + "IAudioClient::SetEventHandle");
    }
    if (transmitCallbackMode) {
        hr = transmitAudioClient->SetEventHandle(speakerEvent);
        EXIT_ON_HR_ERROR(ERROR_TRANSMIT + "IAudioClient::SetEventHandle");
    }

    hr = speakerAudioClient->GetService(
             IID_IAudioRenderClient,
             (void**)&speakerRenderClient);
    EXIT_ON_HR_ERROR(ERROR_SPEAKER + "IAudioClient::GetService(IAudioRenderClient)");

    hr = receiveAudioClient->GetService(
             IID_IAudioCaptureClient,
             (void**)&receiveCaptureClient);
    EXIT_ON_HR_ERROR(ERROR_RECEIVE + "IAudioClient::GetService(IAudioCaptureClient)");

    hr = transmitAudioClient->GetService(
             IID_IAudioRenderClient,
             (void**)&transmitRenderClient);
    EXIT_ON_HR_ERROR(ERROR_TRANSMIT + "IAudioClient::GetService(IAudioRenderClient)");

Exit:
    CoTaskMemFree(transmitID);
    CoTaskMemFree(speakerID);
    CoTaskMemFree(wavex);
    SAFE_RELEASE(pEnumerator);
}


Audio::~Audio()
{
    if (transmitEvent) CloseHandle(transmitEvent);
    SAFE_RELEASE(transmitDevice);
    SAFE_RELEASE(transmitAudioClient);
    SAFE_RELEASE(transmitRenderClient);

    if (receiveEvent) CloseHandle(receiveEvent);
    SAFE_RELEASE(receiveDevice);
    SAFE_RELEASE(receiveAudioClient);
    SAFE_RELEASE(receiveCaptureClient);

    if (speakerEvent) CloseHandle(speakerEvent);
    SAFE_RELEASE(speakerDevice);
    SAFE_RELEASE(speakerAudioClient);
    SAFE_RELEASE(speakerRenderClient);
}

int Audio::mutePaddingFrames()
{
    // Extra 3ms from WASAPI in each receive and transmit buffers
    return transmitBufferFrames +
           receiveBufferFrames +
           0.006 * PEABERRYRATE;
}

qreal Audio::transmitPaddingSecs()
{
    // Extra 3ms is WASPI internal buffer
    return (qreal)transmitBufferFrames / PEABERRYRATE + 0.003;
}

HRESULT Audio::findSpeakerWavex(WAVEFORMATEXTENSIBLE *wavex)
{
    const DWORD sampleRates[] = {48000, 44100, 96000, 24000, 88200, 192000};
    const int sampleRatesQty = sizeof(sampleRates) / sizeof(sampleRates[0]);
    for (int sri = 0; sri < sampleRatesQty; sri++) {
        wavex->Format.nSamplesPerSec = sampleRates[sri];
        for (int channels = 1; channels < 3; channels++) {
            wavex->Format.nChannels = channels;
            if (channels==1) wavex->dwChannelMask = SPEAKER_FRONT_CENTER;
            else wavex->dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
            for (int format = 0; format < 2; format ++) {
                if (!format) {
                    wavex->Format.wBitsPerSample = 32;
                    wavex->Samples.wValidBitsPerSample = 32;
                } else {
                    wavex->Format.wBitsPerSample = 16;
                    wavex->Samples.wValidBitsPerSample = 16;
                }
                wavex->Format.nBlockAlign = (wavex->Format.wBitsPerSample >> 3) * wavex->Format.nChannels;
                wavex->Format.nAvgBytesPerSec = wavex->Format.nBlockAlign * wavex->Format.nSamplesPerSec;
                for (int plain = 0; plain < 2; plain++) {
                    if (!plain) {
                        wavex->Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
                        wavex->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
                        if (!format) wavex->SubFormat = pcw_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
                        else wavex->SubFormat = pcw_KSDATAFORMAT_SUBTYPE_PCM;
                    } else {
                        wavex->Format.cbSize = 0;
                        if (!format) wavex->Format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
                        else wavex->Format.wFormatTag = WAVE_FORMAT_PCM;
                    }
                    HRESULT hr = speakerAudioClient->IsFormatSupported(
                                     AUDCLNT_SHAREMODE_EXCLUSIVE,
                                     (WAVEFORMATEX*)wavex, NULL);
                    if (hr==AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED) return hr;
                    if (SUCCEEDED(hr)) return 0;
                }
            }
        }
    }
    return AUDCLNT_E_UNSUPPORTED_FORMAT;
}

void Audio::findPeaberry(IMMDeviceEnumerator *pEnumerator,
                         bool isTransmit,
                         IMMDevice **device,
                         IAudioClient **audioClient,
                         WAVEFORMATEXTENSIBLE *wavex,
                         const QString &errstr)
{
    HRESULT hr;
    IMMDeviceCollection *pCollection = NULL;
    IPropertyStore *pProps = NULL;
    IMMDevice *pDevice = NULL;
    QString errname;
    EDataFlow dataflow = isTransmit ? eRender : eCapture;
    PROPVARIANT varName;
    PropVariantInit(&varName);

    hr = pEnumerator->EnumAudioEndpoints(
             dataflow, DEVICE_STATE_ACTIVE,
             &pCollection);
    EXIT_ON_HR_ERROR("IMMDeviceEnumerator::EnumAudioEndpoints");

    UINT count;
    hr = pCollection->GetCount(&count);
    EXIT_ON_HR_ERROR("IMMDeviceCollection::GetCount");

    for (UINT i = 0; i < count; i++)
    {
        hr = pCollection->Item(i, &pDevice);
        EXIT_ON_HR_ERROR("IMMDeviceCollection::Item");

        hr = pDevice->OpenPropertyStore(
                 STGM_READ, &pProps);
        EXIT_ON_HR_ERROR("IMMDevice::OpenPropertyStore");

        PropVariantClear(&varName);
        hr = pProps->GetValue(PKEY_DeviceInterface_FriendlyName, &varName);
        EXIT_ON_HR_ERROR("IPropertyStore::GetValue");

        QString devName((QChar*)varName.pwszVal);

        SAFE_RELEASE(pProps);
        if (devName.contains(QStringLiteral("Peaberry Radio"))) {
            *device = pDevice;
            pDevice = NULL;
            break;
        }
        SAFE_RELEASE(pDevice);
    }

    if (!(*device)) EXIT_ERROR(errstr + "device not found.");

    hr = (*device)->Activate(
             IID_IAudioClient, CLSCTX_ALL,
             NULL, (void**)audioClient);
    EXIT_ON_HR_ERROR(errstr + "IMMDevice::Activate");

    wavex->Format.nSamplesPerSec = PEABERRYRATE;
    wavex->Format.nChannels = 2;
    wavex->dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    wavex->Format.wBitsPerSample = 16;
    wavex->Samples.wValidBitsPerSample = 16;
    wavex->Format.nBlockAlign = (wavex->Format.wBitsPerSample >> 3) * wavex->Format.nChannels;
    wavex->Format.nAvgBytesPerSec = wavex->Format.nBlockAlign * wavex->Format.nSamplesPerSec;
    wavex->Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    wavex->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wavex->SubFormat = pcw_KSDATAFORMAT_SUBTYPE_PCM;
    hr = (*audioClient)->IsFormatSupported(
             AUDCLNT_SHAREMODE_EXCLUSIVE,
             (WAVEFORMATEX*)wavex, NULL);
    if (FAILED(hr)) {
        wavex->Format.cbSize = 0;
        wavex->Format.wFormatTag = WAVE_FORMAT_PCM;
        hr = (*audioClient)->IsFormatSupported(
                 AUDCLNT_SHAREMODE_EXCLUSIVE,
                 (WAVEFORMATEX*)wavex, NULL);
    }

    if (hr==AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED) {
        SAFE_RELEASE(*device);
        EXIT_ERROR(errstr + DOES_NOT_ALLOW_EXCLUSIVE);
    }
    if (FAILED(hr)) {
        SAFE_RELEASE(*device);
        EXIT_ERROR(errstr + "device does not support expected format.");
    }

Exit:
    PropVariantClear(&varName);
    SAFE_RELEASE(pDevice);
    SAFE_RELEASE(pProps);
    SAFE_RELEASE(pCollection);
}

// A common problem is that users fiddle with volume controls. Especially
// after Windows decides that your new radio should transmit system sounds.
void Audio::setPeaberryVolume(IMMDevice *device, const QString &errstr)
{
    HRESULT hr;
    IAudioEndpointVolume *pEndptVol = NULL;

    hr = device->Activate(IID_IAudioEndpointVolume,
                          CLSCTX_ALL, NULL, (void**)&pEndptVol);
    EXIT_ON_HR_ERROR(errstr + "IMMDevice::Activate(IAudioEndpointVolume)");

    hr = pEndptVol->SetMute(FALSE, NULL);
    EXIT_ON_HR_ERROR(errstr + "IAudioEndpointVolume::SetMute");

    hr = pEndptVol->SetMasterVolumeLevelScalar(1.0, NULL);
    EXIT_ON_HR_ERROR(errstr + "IAudioEndpointVolume::SetMasterVolumeLevelScalar");

    hr = pEndptVol->SetChannelVolumeLevelScalar(0, 1.0, NULL);
    EXIT_ON_HR_ERROR(errstr + "IAudioEndpointVolume::SetChannelVolumeLevelScalar(0)");

    hr = pEndptVol->SetChannelVolumeLevelScalar(1, 1.0, NULL);
    EXIT_ON_HR_ERROR(errstr + "IAudioEndpointVolume::SetChannelVolumeLevelScalar(1)");

Exit:
    SAFE_RELEASE(pEndptVol);
}

void Audio::initializeAudioClient(IMMDevice *device, WAVEFORMATEX *wfx, IAudioClient **audioClient, bool *callbackMode, UINT32 *bufferSize, const QString &errstr)
{
    HRESULT hr;
    REFERENCE_TIME hnsBufferDuration;
    bool bufferRetried = false;
    DWORD streamFlags = 0;
    if (*callbackMode == true) streamFlags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;

    // I tried to check PKEY_AudioEndpoint_Supports_EventDriven_Mode
    // but the vendor may not have that correctly set in the ini file.
    // Maybe there's a better way than checking for E_INVALIDARG.

    hr = (*audioClient)->GetDevicePeriod(NULL, &hnsBufferDuration);
    EXIT_ON_HR_ERROR(errstr + "IAudioClient::GetDevicePeriod");

    while (true) {
        hr = (*audioClient)->Initialize(
                 AUDCLNT_SHAREMODE_EXCLUSIVE,
                 streamFlags,
                 hnsBufferDuration,
                 hnsBufferDuration,
                 wfx,
                 NULL);
        if (hr==E_INVALIDARG) {
            if (streamFlags==0) EXIT_ERROR(errstr + "IAudioClient::Initialize == E_INVALIDARG");
            streamFlags = 0;
            *callbackMode = false;
        }
        else if (hr==AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
            if (bufferRetried) EXIT_ERROR(errstr + "IAudioClient::Initialize == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED");
            bufferRetried = true;
            hr = (*audioClient)->GetBufferSize(bufferSize);
            EXIT_ON_HR_ERROR(errstr + "audioClient->GetBufferSize");
            hnsBufferDuration = ((10000.0 * 1000 / wfx->nSamplesPerSec * (*bufferSize)) + 0.5);
        }
        else if (SUCCEEDED(hr)) {
            hr = (*audioClient)->GetBufferSize(bufferSize);
            EXIT_ON_HR_ERROR(errstr + "IAudioClient::GetBufferSize");
            break;
        }
        else EXIT_ERROR(errstr + "IAudioClient::Initialize");
        // reset for a retry
        SAFE_RELEASE(*audioClient);
        hr = device->Activate(
                 IID_IAudioClient, CLSCTX_ALL,
                 NULL, (void**)audioClient);
        EXIT_ON_HR_ERROR(errstr + "IMMDevice::Activate");
    }

Exit:
    return;
}


void Audio::start()
{
    worker = new AudioWorkerThread(this);
    worker->start(QThread::TimeCriticalPriority);
}

void Audio::stop()
{
    worker->running = false;
    worker->quit();
    worker->wait();
    delete worker;
}


void AudioWorkerThread::run()
{
    HRESULT hr;
    UINT32 padding;
    HANDLE events[] = {audio->transmitEvent, audio->speakerEvent};
    DWORD taskIndex = 0;
    HANDLE mmtc = AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &taskIndex);
    UINT dwMilliseconds = 5;
    if (!audio->speakerCallbackMode || !audio->transmitCallbackMode)
        dwMilliseconds = 1;
    timeBeginPeriod(dwMilliseconds);
    running = true;

    // Prime buffers before calling start
    // Necessary for some drivers to know callback timing
    doSpeaker(audio->speakerBufferFrames);
    doTransmit(audio->transmitBufferFrames);

    hr = audio->speakerAudioClient->Start();
    if (FAILED(hr)) qFatal("speakerAudioClient->Start");
    hr = audio->receiveAudioClient->Start();
    if (FAILED(hr)) qFatal("receiveAudioClient->Start");
    hr = audio->transmitAudioClient->Start();
    if (FAILED(hr)) qFatal("transmitAudioClient->Start");

    while (running) {
        switch(WaitForMultipleObjects(2, events, false, dwMilliseconds)) {
        case WAIT_OBJECT_0+0:
            doTransmit(audio->transmitBufferFrames);
            break;
        case WAIT_OBJECT_0+1:
            doSpeaker(audio->speakerBufferFrames);
            break;
        case WAIT_TIMEOUT:
            break;
        default:
            qFatal("WaitForMultipleObjects");
            break;
        }
        if (!audio->speakerCallbackMode) {
            hr = audio->speakerAudioClient->GetCurrentPadding(&padding);
            if (SUCCEEDED(hr))
                doSpeaker(audio->speakerBufferFrames - padding);
        }
        if (!audio->transmitCallbackMode) {
            hr = audio->transmitAudioClient->GetCurrentPadding(&padding);
            if (SUCCEEDED(hr))
                doTransmit(audio->transmitBufferFrames - padding);
        }
        doReceive();
    }

    hr = audio->speakerAudioClient->Stop();
    if (FAILED(hr)) qFatal("speakerAudioClient->Stop");
    hr = audio->receiveAudioClient->Stop();
    if (FAILED(hr)) qFatal("receiveAudioClient->Stop");
    hr = audio->transmitAudioClient->Stop();
    if (FAILED(hr)) qFatal("transmitAudioClient->Stop");

    timeEndPeriod(dwMilliseconds);
    if (mmtc) AvRevertMmThreadCharacteristics(mmtc);
}

void AudioWorkerThread::doSpeaker(int frames)
{
    if (!frames) return;
    switch(audio->speakerFormat) {
    case SPEAKER_INT16_MONO:
        doSpeakerGeneric<AudioSpeakerInt16Mono>(frames);
        break;
    case SPEAKER_INT16_STEREO:
        doSpeakerGeneric<AudioSpeakerInt16Stereo>(frames);
        break;
    case SPEAKER_FLOAT_MONO:
        doSpeakerGeneric<AudioSpeakerFloatMono>(frames);
        break;
    case SPEAKER_FLOAT_STEREO:
        doSpeakerGeneric<AudioSpeakerFloatStereo>(frames);
        break;
    }
}

template <class T>
void AudioWorkerThread::doSpeakerGeneric(int frames)
{
    HRESULT hr;
    T *buf;
    int i = 0, samps = audio->speaker.samples;
    if (samps > frames) samps = frames;

    audio->updateBufAverageSize(frames, Audio::BUFLEVEL_ALPHA);

    hr = audio->speakerRenderClient->GetBuffer(frames, (BYTE**)&buf);
    if (FAILED(hr)) return;

    // Begin with window shape
    while (audio->speaker.shapePos < audio->speaker.shape.size() && i < samps) {
        audio->speaker.nco *= audio->speaker.clk;
        buf[i] = audio->speaker.shape[audio->speaker.shapePos]
                 * audio->speaker.nco.real()
                 * audio->speaker.volume
                 + audio->speakerBuffer[++audio->speakerBufferOut];
        i++;
        audio->speaker.shapePos++;
    }
    // Continuous tone
    while (i < samps) {
        audio->speaker.nco *= audio->speaker.clk;
        buf[i] = audio->speaker.nco.real()
                 * audio->speaker.volume
                 + audio->speakerBuffer[++audio->speakerBufferOut];
        i++;
    }
    // Anything sent so far is considered for the timing.
    audio->speaker.samples -= i;
    if (audio->speaker.samples < 0) audio->speaker.samples = 0;
    // End with window shape
    if (!audio->speaker.samples) {
        while (audio->speaker.shapePos && i < frames) {
            audio->speaker.nco *= audio->speaker.clk;
            audio->speaker.shapePos--;
            buf[i] = audio->speaker.shape[audio->speaker.shapePos]
                     * audio->speaker.nco.real()
                     * audio->speaker.volume
                     + audio->speakerBuffer[++audio->speakerBufferOut];
            i++;
        }
    }
    // Keep buffers full
    while (i < frames) {
        buf[i] = audio->speakerBuffer[++audio->speakerBufferOut];
        i++;
    }
    // Success
    hr = audio->speakerRenderClient->ReleaseBuffer(frames, 0);
}

void AudioWorkerThread::doReceive()
{
    HRESULT hr;
    INT16 *buf;
    UINT32 numFramesToRead;
    DWORD dwFlags;

    hr = audio->receiveCaptureClient->GetBuffer((BYTE**)&buf, &numFramesToRead, &dwFlags, NULL, NULL);
    if (!numFramesToRead || FAILED(hr)) return;

    int samples = numFramesToRead * 2, i = 0;
    while (i < samples) {
        if (audio->receiveMuteCount > 0) {
            audio->receiveMuteCount--;
            audio->receiveMuteVolume *= audio->receiveMuteRampDown;
        } else {
            audio->receiveMuteVolume = 1 - audio->receiveMuteRampUp +
                                       audio->receiveMuteRampUp * audio->receiveMuteVolume;
        }
        REAL v2 = buf[i] / 32767.0 * audio->receiveMuteVolume;
        i++;
        REAL v1 = buf[i] / 32767.0 * audio->receiveMuteVolume;
        i++;
        quint16 pos = audio->capturePos++;
        audio->captureRaw[pos] = COMPLEX(v1,v2);
        v1 = v1 - audio->rxBiasReal;
        v2 = v2 - audio->rxBiasImag;
        v1 = v1 + v2 * audio->rxIqPhase;
        v2 = v2 * audio->rxIqGain;
        audio->captureAdj[pos] = COMPLEX(v1,v2);
        if (!(++pos & (PEABERRYSIZE-1))) {
            emit audio->demodUpdate(&audio->captureAdj[(quint16)(pos-PEABERRYSIZE)]);
        }
        if (++audio->captureCount >= PEABERRYRATE/audio->SPECTRUM_FPS) {
            emit audio->spectrumUpdate(audio->captureRaw.data(), audio->captureAdj.data(), audio->capturePos);
            audio->captureCount = 0;
        }
    }
    audio->receiveCaptureClient->ReleaseBuffer(numFramesToRead);
    doReceive();
}

void AudioWorkerThread::doTransmit(int frames)
{
    if (!frames) return;
    HRESULT hr;
    std::complex<INT16> *buf;
    std::complex<qreal> tmp;
    qreal volume = audio->transmit.volume * 32767;
    int i = 0, samps = audio->transmit.samples;
    if (samps > frames) samps = frames;

    hr = audio->transmitRenderClient->GetBuffer(frames, (BYTE**)&buf);
    if (FAILED(hr)) return;

    // Begin with window shape
    while (audio->transmit.shapePos < audio->transmit.shape.size() && i < samps) {
        audio->transmit.nco *= audio->transmit.clk;
        tmp = audio->transmit.nco * (audio->transmit.shape[audio->transmit.shapePos] * volume);
        buf[i] = std::complex<INT16>(tmp.imag() * audio->txIqGain, tmp.real() + tmp.imag() * audio->txIqPhase);
        i++;
        audio->transmit.shapePos++;
    }
    // Continuous tone
    while (i < samps) {
        audio->transmit.nco *= audio->transmit.clk;
        tmp = audio->transmit.nco * volume;
        buf[i] = std::complex<INT16>(tmp.imag() * audio->txIqGain, tmp.real() + tmp.imag() * audio->txIqPhase);
        i++;
    }
    // Anything sent so far is considered for the timing.
    audio->transmit.samples -= i;
    if (audio->transmit.samples < 0) audio->transmit.samples = 0;
    // End with window shape
    if (audio->transmit.samples == 0) {
        while (audio->transmit.shapePos > 0 && i < frames) {
            audio->transmit.nco *= audio->transmit.clk;
            audio->transmit.shapePos--;
            tmp = audio->transmit.nco * (audio->transmit.shape[audio->transmit.shapePos] * volume);
            buf[i] = std::complex<INT16>(tmp.imag() * audio->txIqGain, tmp.real() + tmp.imag() * audio->txIqPhase);
            i++;
        }
    }
    // No tone
    while (i < frames) {
        buf[i] = 0;
        i++;
    }
    hr = audio->transmitRenderClient->ReleaseBuffer(frames, 0);
}
