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
//#include <wrl.h>

#include <SmartPtr.hpp>

#include "IUnknownBase.hpp"
#include "IAudioGenerator.hpp"
#include "DeviceState.hpp"

// using namespace winrt::Windows::Media::Devices;
// using namespace winrt::Windows::Storage::Streams;
// using namespace Microsoft::WRL;

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
    // HRESULT StartPlaybackAsync();
    HRESULT StartPlayback();
    // HRESULT StopPlaybackAsync();
    HRESULT StopPlayback();
    // HRESULT PausePlaybackAsync();
    HRESULT PausePlayback();

    double GetPeriodInSeconds();

    HRESULT SetVolumeOnSession(UINT32 volume);
    DeviceStateChangedEvent& GetDeviceStateEvent() { return m_DeviceStateChanged; };

    // IActivateAudioInterfaceCompletionHandler
    // STDMETHOD(ActivateCompleted)(IActivateAudioInterfaceAsyncOperation* operation);

private:
#if 0
    class AsyncCallback : public IUnknownBase<IMFAsyncCallback>
    {
    public:
        AsyncCallback(std::function<void(IMFAsyncResult* pResult)> callback)
            : _callback(callback)
            , _dwQueueID(MFASYNC_CALLBACK_QUEUE_MULTITHREADED)
        {
        }

        STDMETHOD(GetParameters)(/* [out] */ __RPC__out DWORD* pdwFlags, /* [out] */ __RPC__out DWORD* pdwQueue)
        {
            *pdwFlags = 0;
            *pdwQueue = _dwQueueID;
            return S_OK;
        }
        STDMETHOD(Invoke)(/* [out] */ __RPC__out IMFAsyncResult* pResult)
        {
            _callback(pResult);
            return S_OK;
        }
        void SetQueueID(DWORD dwQueueID) { _dwQueueID = dwQueueID; }

    protected:
        std::function<void(IMFAsyncResult* pResult)> _callback;
        DWORD _dwQueueID;
    };

#endif
    // AsyncCallback m_startPlaybackCb;
    // AsyncCallback m_stopPlaybackCb;
    // AsyncCallback m_pausePlaybackCb;
    // AsyncCallback m_sampleReadyCb;

    // HRESULT OnStartPlayback(IMFAsyncResult* pResult);
    // HRESULT OnStopPlayback(IMFAsyncResult* pResult);
    // HRESULT OnPausePlayback(IMFAsyncResult* pResult);
    // HRESULT OnSampleReady(IMFAsyncResult* pResult);

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
    // MFWORKITEM_KEY m_SampleReadyKey;
    // std::mutex m_mutex;

    WAVEFORMATEX* m_MixFormat;
    /*
    UINT32 m_DefaultPeriodInFrames;
    UINT32 m_FundamentalPeriodInFrames;
    UINT32 m_MaxPeriodInFrames;
    UINT32 m_MinPeriodInFrames;
    UINT32 m_currentSharedPeriodInFrames;
    */
    SmartPtr<IAudioRenderClient> m_AudioRenderClient;
    SmartPtr<IAudioClient3> m_AudioClient;
    SmartPtr<IMMDevice> m_device;
    SmartPtr<IMFAsyncResult> m_SampleReadyAsyncResult;

    DeviceStateChangedEvent m_DeviceStateChanged;
    DEVICEPROPS m_DeviceProps;

    std::shared_ptr<IAudioGenerator> m_toneSource;
};
