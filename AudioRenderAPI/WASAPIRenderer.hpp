#pragma once

//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

//
// wasapirenderer.h
//

#include <memory>
#include <mutex>

#include <mfapi.h>
#include <AudioClient.h>
#include <mmdeviceapi.h>

#include <SmartPtr.hpp>

#include "IUnknownBase.hpp"
#include "IAudioGenerator.hpp"
#include "DeviceState.hpp"

// User Configurable Arguments for Scenario
struct DEVICEPROPS {
    bool IsHWOffload;
    bool IsBackground;
    // bool IsRawSupported;
    // bool IsRawChosen;
    bool IsLowLatency;
    REFERENCE_TIME hnsBufferDuration;
};

// Primary WASAPI Renderering Class
class WASAPIRenderer
{
public:
    WASAPIRenderer();
    ~WASAPIRenderer();

    void SetGenerator(std::shared_ptr<IAudioGenerator> generator) { m_toneSource = generator; }

    HRESULT SetDevice(SmartPtr<IMMDevice> device);
    HRESULT SetProperties(DEVICEPROPS props);
    HRESULT InitializeAudioDevice();
    HRESULT StartPlayback();
    HRESULT StopPlayback();
    HRESULT PausePlayback();

    double GetPeriodInSeconds();

    HRESULT SetVolumeOnSession(UINT32 volume);
    DeviceStateChangedEvent& GetDeviceStateEvent() { return m_DeviceStateChanged; };

private:

    HRESULT ConfigureDeviceInternal();
    // HRESULT ValidateBufferValue();
    HRESULT OnAudioSampleRequested(bool IsSilence = false);
    HRESULT ConfigureSource();
    UINT32 GetBufferFramesPerPeriod();

    HRESULT GetToneSample(UINT32 FramesAvailable);

    void RenderLoop();
    static void RenderLoopThreadMain(WASAPIRenderer* _this);

private:
    std::thread m_renderThread;
    UINT32 m_BufferFrames;
    HANDLE m_SampleReadyEvent;

    WAVEFORMATEX* m_MixFormat;

    SmartPtr<IAudioRenderClient> m_AudioRenderClient;
    SmartPtr<IAudioClient3> m_AudioClient;
    SmartPtr<IMMDevice> m_device;
    SmartPtr<IMFAsyncResult> m_SampleReadyAsyncResult;

    DeviceStateChangedEvent m_DeviceStateChanged;
    DEVICEPROPS m_DeviceProps;

    std::shared_ptr<IAudioGenerator> m_toneSource;
};
