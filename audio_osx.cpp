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

#include "audio_osx.h"
#include "radio.h"

Audio::Audio(Radio *radio) :
    AudioBase(radio),
    speakerInstance(nullptr),
    peaberryInstance(nullptr)
{
    AudioDeviceID speakerID;
    AudioDeviceID peaberryID;

    findDevices(speakerID, peaberryID);
    if (!error.isEmpty()) return;

    AudioComponent comp;
    AudioComponentDescription desc;

    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;

    comp = AudioComponentFindNext(NULL, &desc);
    if (comp == NULL) {
        error = "Failed to find Audio HAL component.";
        return;
    }

    configureSpeaker(comp, speakerID);
    if (!error.isEmpty()) return;

    configurePeabery(comp, peaberryID);
    if (!error.isEmpty()) return;

    speakerBufLevel = bufAverageTarget();
}

Audio::~Audio()
{
    if (peaberryInstance) {
        AudioUnitUninitialize(peaberryInstance);
        AudioComponentInstanceDispose(peaberryInstance);
    }
    if (speakerInstance) {
        AudioUnitUninitialize(speakerInstance);
        AudioComponentInstanceDispose(speakerInstance);
    }
}

void Audio::findDevices(AudioDeviceID &speakerID, AudioDeviceID &peaberryID)
{
    int radiosFound = 0;
    OSStatus status;
    UInt32 size;

    AudioObjectPropertyAddress propAddress;
    propAddress = { kAudioHardwarePropertyDefaultOutputDevice,
                    kAudioObjectPropertyScopeGlobal,
                    kAudioObjectPropertyElementMaster
                  };
    size = sizeof(speakerID);
    status = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                        &propAddress,
                                        0, NULL,
                                        &size, &speakerID);
    if (status != noErr) {
        error = "Unable to find default audio output device.";
        return;
    }

    size = 0;
    propAddress = { kAudioHardwarePropertyDevices,
                    kAudioObjectPropertyScopeGlobal,
                    kAudioObjectPropertyElementMaster
                  };
    status = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
                                            &propAddress,
                                            0, NULL,
                                            &size);
    if (status == noErr) {
        const int dc = size / sizeof(AudioDeviceID);
        if (dc > 0) {
            AudioDeviceID*  audioDevices = new AudioDeviceID[dc];
            status = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                                &propAddress,
                                                0, NULL,
                                                &size, audioDevices);
            if (status == noErr) {
                for (int i = 0; i < dc; ++i) {
                    CFStringRef name;
                    size = sizeof(CFStringRef);
                    propAddress = { kAudioObjectPropertyName,
                                    kAudioObjectPropertyScopeGlobal,
                                    kAudioObjectPropertyElementMaster
                                  };
                    status = AudioObjectGetPropertyData(audioDevices[i],
                                                        &propAddress,
                                                        0, NULL,
                                                        &size, &name);
                    if (status == noErr) {
                        if (convCFStringToQString(name).contains(QStringLiteral("Peaberry Radio"))) {
                            radiosFound++;
                            peaberryID = audioDevices[i];
                        }
                        CFRelease(name);
                    }
                }
            }
            delete[] audioDevices;
        }
    }

    if (radiosFound == 0) {
        error = "Peaberry Radio audio device not found.";
        return;
    }
    if (radiosFound > 1) {
        error = "Too many Peaberry Radio audio devices found.";
        return;
    }
    if (speakerID == peaberryID) {
        error = "Peaberry Radio audio device is currently "
                "set as the default output device. "
                "You can fix this in System Preferences or with "
                "the Audio MIDI Setup utility.";
        return;
    }
}

QString Audio::convCFStringToQString(CFStringRef str)
{
    if (!str) return QString();
    CFIndex length = CFStringGetLength(str);
    if (length == 0) return QString();
    QString string(length, Qt::Uninitialized);
    CFStringGetCharacters(str, CFRangeMake(0, length),
                          reinterpret_cast<UniChar*>
                          (const_cast<QChar*>(string.unicode())));
    return string;
}

UInt32 Audio::createInstance(AudioComponent comp,
                             AudioDeviceID devID,
                             AudioComponentInstance *instance,
                             AudioStreamBasicDescription *format,
                             bool isRadio)
{
    OSStatus status;
    UInt32 size;

    status = AudioComponentInstanceNew(comp, instance);
    if (status != noErr) {
        error = "Faled to create component instance.";
        return 0;
    }

    UInt32 enableIO;
    enableIO = isRadio ? 1 : 0;
    status = AudioUnitSetProperty(*instance,
                                  kAudioOutputUnitProperty_EnableIO,
                                  kAudioUnitScope_Input,
                                  1, // input element
                                  &enableIO,
                                  sizeof(enableIO));

    if (status != noErr) {
        error = "Failed to set capture scope.";
        return 0;
    }

    enableIO = 1;
    status = AudioUnitSetProperty(*instance,
                                  kAudioOutputUnitProperty_EnableIO,
                                  kAudioUnitScope_Output,
                                  0,   //output element
                                  &enableIO,
                                  sizeof(enableIO));
    if (status != noErr) {
        error = "Failed to set render scope.";
        return 0;
    }

    status = AudioUnitSetProperty(*instance,
                                  kAudioOutputUnitProperty_CurrentDevice,
                                  kAudioUnitScope_Global,
                                  0,
                                  &devID,
                                  sizeof(AudioDeviceID));
    if (status != noErr) {
        error = "Failed to set device ID.";
        return 0;
    }

    size = sizeof(AudioStreamBasicDescription);
    status = AudioUnitGetProperty (*instance,
                                   kAudioUnitProperty_StreamFormat,
                                   kAudioUnitScope_Global,
                                   0,
                                   format,
                                   &size);
    if (status != noErr) {
        error = "Failed to get audio format.";
        return 0;
    }

    if (isRadio && format->mSampleRate != PEABERRYRATE) {
        error = "Sample rate not 96000 Hz.";
        return 0;
    }

    if (isRadio && format->mChannelsPerFrame < 2) {
        error = "Not enough channels.";
        return 0;
    }

    format->mFormatID         = kAudioFormatLinearPCM;
    format->mFormatFlags      = kAudioFormatFlagIsFloat |
                                kAudioFormatFlagIsPacked |
                                kAudioFormatFlagsNativeEndian;
    format->mFramesPerPacket  = 1;
    format->mBitsPerChannel   = 32;
    format->mChannelsPerFrame = isRadio ? 2 : 1;
    format->mBytesPerFrame    = format->mChannelsPerFrame * (format->mBitsPerChannel / 8);
    format->mBytesPerPacket   = format->mFramesPerPacket * format->mBytesPerFrame;

    status = AudioUnitSetProperty(*instance,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Input,
                                  0,
                                  format,
                                  size);
    if (status != noErr) {
        error = "Failed to set render format.";
        return 0;
    }

    if (isRadio) {
        status = AudioUnitSetProperty(*instance,
                                      kAudioUnitProperty_StreamFormat,
                                      kAudioUnitScope_Output,
                                      1,
                                      format,
                                      size);
        if (status != noErr) {
            error = "Failed to set capture format.";
            return 0;
        }
    }

    AudioObjectPropertyAddress propAddress = { kAudioDevicePropertyBufferFrameSizeRange,
                                               kAudioObjectPropertyScopeGlobal,
                                               kAudioObjectPropertyElementMaster
                                             };
    AudioValueRange frameRange = { 0, 0 };
    size = sizeof(AudioValueRange);
    status = AudioObjectGetPropertyData(devID,
                                        &propAddress,
                                        0,
                                        NULL,
                                        &size,
                                        &frameRange);
    if (status != noErr) {
        error = "Failed to get buffer size range";
        return 0;
    }

    UInt32 numberOfFrames = format->mSampleRate * OUTPUT_INTERVAL;
    if (numberOfFrames < frameRange.mMinimum) numberOfFrames = frameRange.mMinimum;
    if (numberOfFrames > frameRange.mMaximum) numberOfFrames = frameRange.mMaximum;
    status = AudioUnitSetProperty(*instance,
                                  kAudioDevicePropertyBufferFrameSize,
                                  kAudioUnitScope_Global,
                                  0,
                                  &numberOfFrames,
                                  sizeof(numberOfFrames));
    if (status != noErr) {
        error = "Failed to set render buffer size";
        return 0;
    }

    if (AudioUnitInitialize(*instance)) {
        error = "Failed to initialize AudioUnit";
        return 0;
    }

    return numberOfFrames;
}


void Audio::configureSpeaker(AudioComponent comp, AudioDeviceID speakerID)
{
    OSStatus status;
    AudioStreamBasicDescription format;

    createInstance(comp, speakerID, &speakerInstance, &format, false);
    if (!error.isEmpty()) {
        error.insert(0, "Speaker Audio: ");
        return;
    }

    speakerSampleRate = format.mSampleRate;

    AURenderCallbackStruct  cb;
    cb.inputProc = speakerCallback;
    cb.inputProcRefCon = this;
    status = AudioUnitSetProperty(speakerInstance,
                                  kAudioUnitProperty_SetRenderCallback,
                                  kAudioUnitScope_Global,
                                  0,
                                  &cb,
                                  sizeof(cb));
    if (status != noErr) {
        error = "Speaker Audio: Failed to set render callback.";
        return;
    }
}

void Audio::configurePeabery(AudioComponent comp, AudioDeviceID peaberryID)
{
    OSStatus status;
    AudioStreamBasicDescription format;

    UInt32 numFrames = createInstance(comp, peaberryID, &peaberryInstance, &format, true);
    if (!error.isEmpty()) {
        error.insert(0, "Peaberry Radio: ");
        return;
    }

    captureBufData.resize(numFrames*2);
    captureBufferList.mNumberBuffers = 1;
    captureBufferList.mBuffers[0].mNumberChannels = 2;
    captureBufferList.mBuffers[0].mDataByteSize = captureBufData.size() * sizeof(captureBufData[0]);
    captureBufferList.mBuffers[0].mData = captureBufData.data();

    AURenderCallbackStruct  cb;
    cb.inputProc = transmitCallback;
    cb.inputProcRefCon = this;
    status = AudioUnitSetProperty(peaberryInstance,
                                  kAudioUnitProperty_SetRenderCallback,
                                  kAudioUnitScope_Global,
                                  0,
                                  &cb,
                                  sizeof(cb));
    if (status != noErr) {
        error = "Speaker Audio: Failed to set render callback.";
        return;
    }

    cb.inputProc = receiveCallback;
    cb.inputProcRefCon = this;
    status = AudioUnitSetProperty(peaberryInstance,
                                  kAudioOutputUnitProperty_SetInputCallback,
                                  kAudioUnitScope_Global,
                                  0,
                                  &cb,
                                  sizeof(cb));
    if (status != noErr) {
        error = "Speaker Audio: Failed to set capture callback.";
        return;
    }

}

OSStatus Audio::speakerCallback(void *inRefCon,
                                AudioUnitRenderActionFlags *ioActionFlags,
                                const AudioTimeStamp *inTimeStamp,
                                UInt32 inBusNumber,
                                UInt32 inNumberFrames,
                                AudioBufferList *ioData)
{
    Q_UNUSED(ioActionFlags);
    Q_UNUSED(inTimeStamp);
    Q_UNUSED(inBusNumber);
    Audio *audio = (Audio*)inRefCon;
    int i = 0;
    qreal volume = audio->speaker.volume;
    int samps = audio->speaker.samples;
    Float32 *data = (Float32*)ioData->mBuffers[0].mData;
    int freesamps = inNumberFrames;
    if (samps > freesamps) samps = freesamps;

    audio->updateBufAverageSize(freesamps, BUFLEVEL_ALPHA);

    // Begin with window shape
    while (audio->speaker.shapePos < audio->speaker.shape.size() && i < samps) {
        audio->speaker.nco *= audio->speaker.clk;
        data[i] = audio->speaker.shape[audio->speaker.shapePos] *
                  audio->speaker.nco.real() *
                  volume +
                  audio->speakerBuffer[++audio->speakerBufferOut];
        i++;
        audio->speaker.shapePos++;
    }
    // Continuous tone
    while (i < samps) {
        audio->speaker.nco *= audio->speaker.clk;
        data[i] = audio->speaker.nco.real() * volume + audio->speakerBuffer[++audio->speakerBufferOut];;
        i++;
    }
    // Anything sent so far is considered for the timing.
    audio->speaker.samples -= i;
    if (audio->speaker.samples < 0) audio->speaker.samples = 0;
    // End with window shape
    if (!audio->speaker.samples) {
        while (audio->speaker.shapePos && i < freesamps) {
            audio->speaker.nco *= audio->speaker.clk;
            audio->speaker.shapePos--;
            data[i] = audio->speaker.shape[audio->speaker.shapePos] *
                      audio->speaker.nco.real() *
                      volume +
                      audio->speakerBuffer[++audio->speakerBufferOut];;
            i++;
        }
    }
    // No tone
    while (i < freesamps) {
        data[i] = audio->speakerBuffer[++audio->speakerBufferOut];
        i++;
    }
    return noErr;
}


OSStatus Audio::transmitCallback(void *inRefCon,
                                 AudioUnitRenderActionFlags *ioActionFlags,
                                 const AudioTimeStamp *inTimeStamp,
                                 UInt32 inBusNumber,
                                 UInt32 inNumberFrames,
                                 AudioBufferList *ioData)
{
    Q_UNUSED(ioActionFlags);
    Q_UNUSED(inTimeStamp);
    Q_UNUSED(inBusNumber);
    Audio *audio = (Audio*)inRefCon;
    int i = 0;
    qreal volume = audio->transmit.volume;
    int samps = audio->transmit.samples;
    std::complex<Float32> *data = (std::complex<Float32>*)ioData->mBuffers[0].mData;
    std::complex<Float32> tmp;
    int freesamps = inNumberFrames;
    if (samps > freesamps) samps = freesamps;

    // Begin with window shape
    while (audio->transmit.shapePos < audio->transmit.shape.size() && i < samps) {
        audio->transmit.nco *= audio->transmit.clk;
        tmp = audio->transmit.nco * (audio->transmit.shape[audio->transmit.shapePos] * volume);
        data[i] = std::complex<Float32>(tmp.imag() * audio->txIqGain, tmp.real() + tmp.imag() * audio->txIqPhase);
        i++;
        audio->transmit.shapePos++;
    }
    // Continuous tone
    while (i < samps) {
        audio->transmit.nco *= audio->transmit.clk;
        tmp = audio->transmit.nco * volume;
        data[i] = std::complex<Float32>(tmp.imag() * audio->txIqGain, tmp.real() + tmp.imag() * audio->txIqPhase);
        i++;
    }
    // Anything sent so far is considered for the timing.
    audio->transmit.samples -= i;
    if (audio->transmit.samples < 0) audio->transmit.samples = 0;
    // End with window shape
    if (!audio->transmit.samples) {
        while (audio->transmit.shapePos && i < freesamps) {
            audio->transmit.nco *= audio->transmit.clk;
            audio->transmit.shapePos--;
            tmp = audio->transmit.nco * (audio->transmit.shape[audio->transmit.shapePos] * volume);
            data[i] = std::complex<Float32>(tmp.imag() * audio->txIqGain, tmp.real() + tmp.imag() * audio->txIqPhase);
            i++;
        }
    }
    // No tone
    while (i < freesamps) {
        data[i] = 0;
        i++;
    }
    return noErr;
}


OSStatus Audio::receiveCallback(void *inRefCon,
                                AudioUnitRenderActionFlags *ioActionFlags,
                                const AudioTimeStamp *inTimeStamp,
                                UInt32 inBusNumber,
                                UInt32 inNumberFrames,
                                AudioBufferList *ioData)
{
    Q_UNUSED(ioActionFlags);
    Q_UNUSED(inTimeStamp);
    Q_UNUSED(inBusNumber);
    Q_UNUSED(ioData);
    Audio *audio = (Audio*)inRefCon;
    OSStatus err;

    err = AudioUnitRender(audio->peaberryInstance,
                          ioActionFlags,
                          inTimeStamp,
                          inBusNumber,
                          inNumberFrames,
                          &audio->captureBufferList);
    if (err != noErr) return err;

    int samples = inNumberFrames * 2, i = 0;
    while (i < samples) {
        if (audio->receiveMuteCount > 0) {
            audio->receiveMuteCount--;
            audio->receiveMuteVolume *= audio->receiveMuteRampDown;
        } else {
            audio->receiveMuteVolume = 1 - audio->receiveMuteRampUp +
                                       audio->receiveMuteRampUp * audio->receiveMuteVolume;
        }
        REAL v2 = audio->captureBufData[i] * audio->receiveMuteVolume;
        i++;
        REAL v1 = audio->captureBufData[i] * audio->receiveMuteVolume;
        i++;
        quint16 pos = audio->capturePos++;
        audio->captureRaw[pos] = COMPLEX(v1, v2);
        v1 = v1 - audio->rxBiasReal;
        v2 = v2 - audio->rxBiasImag;
        v1 = v1 + v2 * audio->rxIqPhase;
        v2 = v2 * audio->rxIqGain;
        audio->captureAdj[pos] = COMPLEX(v1, v2);
        if (!(++pos & (PEABERRYSIZE-1))) {
            emit audio->demodUpdate(&audio->captureAdj[(quint16)(pos-PEABERRYSIZE)]);
        }
        if (++audio->captureCount >= PEABERRYRATE/SPECTRUM_FPS) {
            emit audio->spectrumUpdate(audio->captureRaw.data(), audio->captureAdj.data(), audio->capturePos);
            audio->captureCount = 0;
        }
    }

    return err;
}

void Audio::start()
{
    AudioOutputUnitStart(speakerInstance);
    AudioOutputUnitStart(peaberryInstance);
}

void Audio::stop()
{
    AudioOutputUnitStop(peaberryInstance);
    AudioOutputUnitStop(speakerInstance);
}
