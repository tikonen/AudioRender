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
#include <wrl.h>

#include <SmartPtr.hpp>

#include "IUnknownBase.hpp"
#include "IAudioGenerator.hpp"
#include "DeviceState.hpp"

using namespace winrt::Windows::Media::Devices;
using namespace winrt::Windows::Storage::Streams;
using namespace Microsoft::WRL;

// enum class ContentType { ContentTypeTone, ContentTypeFile };

// User Configurable Arguments for Scenario
struct DEVICEPROPS {
    bool IsHWOffload;
    bool IsBackground;
    bool IsRawSupported;
    bool IsRawChosen;
    bool IsLowLatency;
    REFERENCE_TIME hnsBufferDuration;
    // IRandomAccessStream ContentStream;
};

// Primary WASAPI Renderering Class
class WASAPIRenderer : public RuntimeClass<RuntimeClassFlags<ClassicCom>, FtmBase, IActivateAudioInterfaceCompletionHandler>
{
public:
    WASAPIRenderer();

    void SetGenerator(std::shared_ptr<IAudioGenerator> generator) { m_toneSource = generator; }

    HRESULT SetDeviceId(winrt::hstring deviceId);
    HRESULT SetProperties(DEVICEPROPS props);
    HRESULT InitializeAudioDeviceAsync();
    HRESULT StartPlaybackAsync();
    HRESULT StopPlaybackAsync();
    HRESULT PausePlaybackAsync();

    double GetPeriodInSeconds();

    HRESULT SetVolumeOnSession(UINT32 volume);
    DeviceStateChangedEvent& GetDeviceStateEvent() { return m_DeviceStateChanged; };

    // IActivateAudioInterfaceCompletionHandler
    STDMETHOD(ActivateCompleted)(IActivateAudioInterfaceAsyncOperation* operation);

private:
    ~WASAPIRenderer();

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

    AsyncCallback m_startPlaybackCb;
    AsyncCallback m_stopPlaybackCb;
    AsyncCallback m_pausePlaybackCb;
    AsyncCallback m_sampleReadyCb;

    HRESULT OnStartPlayback(IMFAsyncResult* pResult);
    HRESULT OnStopPlayback(IMFAsyncResult* pResult);
    HRESULT OnPausePlayback(IMFAsyncResult* pResult);
    HRESULT OnSampleReady(IMFAsyncResult* pResult);

    HRESULT ConfigureDeviceInternal();
    HRESULT ValidateBufferValue();
    HRESULT OnAudioSampleRequested(bool IsSilence = false);
    HRESULT ConfigureSource();
    UINT32 GetBufferFramesPerPeriod();

    HRESULT GetToneSample(UINT32 FramesAvailable);

private:
    winrt::hstring m_DeviceIdString;
    UINT32 m_BufferFrames;
    HANDLE m_SampleReadyEvent;
    MFWORKITEM_KEY m_SampleReadyKey;
    std::mutex m_mutex;

    WAVEFORMATEX* m_MixFormat;
    UINT32 m_DefaultPeriodInFrames;
    UINT32 m_FundamentalPeriodInFrames;
    UINT32 m_MaxPeriodInFrames;
    UINT32 m_MinPeriodInFrames;
    UINT32 m_currentSharedPeriodInFrames;

    SmartPtr<IAudioClient3> m_AudioClient;
    SmartPtr<IAudioRenderClient> m_AudioRenderClient;
    SmartPtr<IMFAsyncResult> m_SampleReadyAsyncResult;

    DeviceStateChangedEvent m_DeviceStateChanged;
    DEVICEPROPS m_DeviceProps;

    std::shared_ptr<IAudioGenerator> m_toneSource;
};
