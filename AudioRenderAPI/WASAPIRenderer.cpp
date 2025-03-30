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
#include "pch.h"

#include <Log.hpp>

#include "WASAPIRenderer.hpp"

//
//  WASAPIRenderer()
//
WASAPIRenderer::WASAPIRenderer()
    : m_BufferFrames(0)
    , m_SampleReadyEvent(NULL)
    , m_MixFormat(nullptr)
    , m_DeviceProps{0}
{
    m_MixFormat = (WAVEFORMATEX*)malloc(sizeof(WAVEFORMATEX));
    memset(m_MixFormat, 0, sizeof(WAVEFORMATEX));

    // Create events for sample ready or user stop
    m_SampleReadyEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
}

WASAPIRenderer::~WASAPIRenderer()
{
    if (m_SampleReadyEvent) CloseHandle(m_SampleReadyEvent);
    m_SampleReadyEvent = NULL;
    free(m_MixFormat);
}


HRESULT WASAPIRenderer::InitializeAudioDevice()
{
    HRESULT hr = S_OK;

    hr = m_device->Activate(__uuidof(IAudioClient3), CLSCTX_ALL, NULL, (void**)&m_AudioClient);

    if (FAILED(hr)) {
        m_DeviceStateChanged.SetState(DeviceState::DeviceStateInError, hr, true);
        LOGE("Activate %s", HResultToString(hr));
        goto exit;
    }

    if (m_DeviceStateChanged.GetState() != DeviceState::DeviceStateUnInitialized) {
        hr = E_NOT_VALID_STATE;
        goto exit;
    }

    m_DeviceStateChanged.SetState(DeviceState::DeviceStateActivated, S_OK, false);

    // Configure user defined properties
    hr = ConfigureDeviceInternal();
    if (FAILED(hr)) {
        goto exit;
    }

#if 0        
        hr = m_AudioClient->InitializeSharedAudioStream(AUDCLNT_STREAMFLAGS_EVENTCALLBACK, m_MinPeriodInFrames, m_MixFormat, nullptr);

        if (FAILED(hr)) {
            goto exit;
        }
        
        hr = m_AudioClient->GetCurrentSharedModeEnginePeriod(&m_MixFormat, &m_currentSharedPeriodInFrames);
        if (FAILED(hr)) {
            goto exit;
        }
#endif
    // AUDCLNT_STREAMFLAGS_RATEADJUST ??
    hr = m_AudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, m_DeviceProps.hnsBufferDuration * 2, 0, m_MixFormat, nullptr);

    if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
        // Align the buffer if needed, see IAudioClient::Initialize() documentation
        UINT32 nFrames = 0;
        hr = m_AudioClient->GetBufferSize(&nFrames);
        if (FAILED(hr)) {
            goto exit;
        }
        m_AudioClient = nullptr;

        hr = m_device->Activate(__uuidof(IAudioClient3), CLSCTX_ALL, NULL, (void**)&m_AudioClient);
        if (FAILED(hr)) {
            goto exit;
        }

        // 20ms
        const double duration = 20.0 * 10000;
        REFERENCE_TIME hnsRequestedDuration = (REFERENCE_TIME)(duration / m_MixFormat->nSamplesPerSec * nFrames + 0.5);
        hr = m_AudioClient->Initialize(
            AUDCLNT_SHAREMODE_EXCLUSIVE, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, hnsRequestedDuration, hnsRequestedDuration, m_MixFormat, nullptr);
    }

    if (FAILED(hr)) {
        goto exit;
    }

    /*
    hr = m_AudioClient->GetCurrentSharedModeEnginePeriod(&m_MixFormat, &m_currentSharedPeriodInFrames);
    if (FAILED(hr)) {
        goto exit;
    }
    */

    // Get the maximum size of the AudioClient Buffer
    hr = m_AudioClient->GetBufferSize(&m_BufferFrames);
    if (FAILED(hr)) {
        goto exit;
    }

    // Get the render client
    hr = m_AudioClient->GetService(__uuidof(IAudioRenderClient), (void**)&m_AudioRenderClient);
    if (FAILED(hr)) {
        goto exit;
    }

    // Sets the event handle that the system signals when an audio buffer is ready to be processed by the client
    hr = m_AudioClient->SetEventHandle(m_SampleReadyEvent);
    if (FAILED(hr)) {
        goto exit;
    }

    // Everything succeeded
    m_DeviceStateChanged.SetState(DeviceState::DeviceStateInitialized, S_OK, true);

exit:
    if (FAILED(hr)) {
        m_DeviceStateChanged.SetState(DeviceState::DeviceStateInError, hr, true);
        m_device = nullptr;
        m_AudioClient = nullptr;
        m_AudioRenderClient = nullptr;
        m_SampleReadyAsyncResult = nullptr;
    }

    return hr;
}


HRESULT WASAPIRenderer::SetDevice(SmartPtr<IMMDevice> device)
{
    m_device = device;
    return S_OK;
}

//
//  SetProperties()
//
//  Sets various properties that the user defines in the scenario
//
HRESULT WASAPIRenderer::SetProperties(DEVICEPROPS props)
{
    m_DeviceProps = props;
    return S_OK;
}

//
//  GetBufferFramesPerPeriod()
//
//  Get the time in seconds between passes of the audio device
//
UINT32 WASAPIRenderer::GetBufferFramesPerPeriod()
{
    double devicePeriodInSeconds = GetPeriodInSeconds();

    return static_cast<UINT32>(m_MixFormat->nSamplesPerSec * devicePeriodInSeconds + 0.5);
}

double WASAPIRenderer::GetPeriodInSeconds()
{
    REFERENCE_TIME defaultDevicePeriod = 0;
    REFERENCE_TIME minimumDevicePeriod = 0;
    HRESULT hr = m_AudioClient->GetDevicePeriod(&defaultDevicePeriod, &minimumDevicePeriod);
    if (FAILED(hr)) {
        return 0;
    }
    return defaultDevicePeriod / (10000.0 * 1000.0);
}

//
//  ConfigureDeviceInternal()
//
//  Sets additional playback parameters and opts into hardware offload
//
HRESULT WASAPIRenderer::ConfigureDeviceInternal()
{
    if (m_DeviceStateChanged.GetState() != DeviceState::DeviceStateActivated) {
        return E_NOT_VALID_STATE;
    }

    HRESULT hr = S_OK;
#if 0
    // Opt into HW Offloading.  If the endpoint does not support offload it will return AUDCLNT_E_ENDPOINT_OFFLOAD_NOT_CAPABLE
    AudioClientProperties audioProps = {0};
    audioProps.cbSize = sizeof(AudioClientProperties);
    //audioProps.bIsOffload = m_DeviceProps.IsHWOffload;
    audioProps.eCategory = AudioCategory_Media;

    //if (m_DeviceProps.IsRawChosen && m_DeviceProps.IsRawSupported) {
        audioProps.Options = AUDCLNT_STREAMOPTIONS_RAW;
    //}

    hr = m_AudioClient->SetClientProperties(&audioProps);
    if (FAILED(hr)) {
        LOG("Failed to set IAudioClient properties\n");
        //return hr;
    }
#endif
    m_MixFormat->cbSize = 0;
    m_MixFormat->wFormatTag = WAVE_FORMAT_PCM;
    m_MixFormat->nChannels = 2;
    m_MixFormat->wBitsPerSample = 16;
    m_MixFormat->nSamplesPerSec = 48000;
    m_MixFormat->nBlockAlign = 4;
    m_MixFormat->nAvgBytesPerSec = m_MixFormat->nBlockAlign * m_MixFormat->nSamplesPerSec;

    WAVEFORMATEX* closestMatch;
    hr = m_AudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, m_MixFormat, &closestMatch);
    if (FAILED(hr)) {
        return hr;
    }
    if (hr == S_FALSE) {
        free(m_MixFormat);
        size_t size = sizeof(WAVEFORMATEX) + closestMatch->cbSize;
        m_MixFormat = (WAVEFORMATEX*)malloc(size);
        memcpy(m_MixFormat, closestMatch, size);
        CoTaskMemFree(closestMatch);
        closestMatch = NULL;
    }

    hr = m_AudioClient->GetDevicePeriod(&m_DeviceProps.hnsBufferDuration, NULL);
    if (FAILED(hr)) {
        return hr;
    }

#if 0
    // This sample opens the device is shared mode so we need to find the supported WAVEFORMATEX mix format
    hr = m_AudioClient->GetMixFormat(&m_MixFormat);
    if (FAILED(hr)) {
        return hr;
    }

    // The wfx parameter below is optional (Its needed only for MATCH_FORMAT clients). Otherwise, wfx will be assumed
    // to be the current engine format based on the processing mode for this stream
    hr = m_AudioClient->GetSharedModeEnginePeriod(
        m_MixFormat, &m_DefaultPeriodInFrames, &m_FundamentalPeriodInFrames, &m_MinPeriodInFrames, &m_MaxPeriodInFrames);
    if (FAILED(hr)) {
        return hr;
    }

    // Verify the user defined value for hardware buffer
    hr = ValidateBufferValue();
#endif

    return hr;
}

//
//  SetVolumeOnSession()
//
HRESULT WASAPIRenderer::SetVolumeOnSession(UINT32 volume)
{
    if (volume > 100) {
        return E_INVALIDARG;
    }

    HRESULT hr = S_OK;
    SmartPtr<ISimpleAudioVolume> SessionAudioVolume = nullptr;
    float ChannelVolume = 0.0;

    hr = m_AudioClient->GetService(__uuidof(ISimpleAudioVolume), reinterpret_cast<void**>(&SessionAudioVolume));
    if (FAILED(hr)) {
        goto exit;
    }

    ChannelVolume = volume / (float)100.0;

    // Set the session volume on the endpoint
    hr = SessionAudioVolume->SetMasterVolume(ChannelVolume, nullptr);

exit:
    return hr;
}

//
//  ConfigureSource()
//
//  Configures tone or file playback
//
HRESULT WASAPIRenderer::ConfigureSource()
{
    HRESULT hr = S_OK;
    UINT32 FramesPerPeriod = GetBufferFramesPerPeriod();

    // Generate the sine wave sample buffer
    hr = m_audioSource->Initialize(FramesPerPeriod, m_MixFormat);

    return hr;
}

HRESULT WASAPIRenderer::StartPlayback()
{
    HRESULT hr = S_OK;

    if (m_DeviceStateChanged.GetState() == DeviceState::DeviceStatePlaying) {
        return hr;
    }

    // We should be stopped if the user stopped playback, or we should be
    // initialzied if this is the first time through getting ready to playback.
    if ((m_DeviceStateChanged.GetState() == DeviceState::DeviceStateStopped) || (m_DeviceStateChanged.GetState() == DeviceState::DeviceStateInitialized)) {
        hr = ConfigureSource();
        if (FAILED(hr)) {
            m_DeviceStateChanged.SetState(DeviceState::DeviceStateInError, hr, true);
            return hr;
        }

        m_DeviceStateChanged.SetState(DeviceState::DeviceStateStarting, S_OK, true);

    } else if (m_DeviceStateChanged.GetState() == DeviceState::DeviceStatePaused) {
    }
    // Pre-Roll the buffer with silence
    hr = OnAudioSampleRequested(true);
    if (FAILED(hr)) {
        goto exit;
    }

    // Actually start the playback
    hr = m_AudioClient->Start();
    if (SUCCEEDED(hr)) {
        m_DeviceStateChanged.SetState(DeviceState::DeviceStatePlaying, S_OK, true);
        m_renderThread = std::thread(&WASAPIRenderer::RenderLoopThreadMain, this);
    }

exit:
    if (FAILED(hr)) {
        m_DeviceStateChanged.SetState(DeviceState::DeviceStateInError, hr, true);
    }
    return hr;
}

HRESULT WASAPIRenderer::StopPlayback()
{
    if ((m_DeviceStateChanged.GetState() != DeviceState::DeviceStatePlaying) && (m_DeviceStateChanged.GetState() != DeviceState::DeviceStatePaused) &&
        (m_DeviceStateChanged.GetState() != DeviceState::DeviceStateInError)) {
        return E_NOT_VALID_STATE;
    }

    m_DeviceStateChanged.SetState(DeviceState::DeviceStateStopping, S_OK, true);

    if (m_renderThread.joinable()) {
        SetEvent(m_SampleReadyEvent);  // wake up worker thread
        m_renderThread.join();
    }

    // Flush anything left in buffer with silence
    OnAudioSampleRequested(true);

    m_AudioClient->Stop();
    m_SampleReadyAsyncResult = nullptr;

    // Flush remaining buffers
    m_audioSource->Flush();

    m_DeviceStateChanged.SetState(DeviceState::DeviceStateStopped, S_OK, true);

    return S_OK;
}

HRESULT WASAPIRenderer::PausePlayback()
{
    if ((m_DeviceStateChanged.GetState() != DeviceState::DeviceStatePlaying) && (m_DeviceStateChanged.GetState() != DeviceState::DeviceStateInError)) {
        return E_NOT_VALID_STATE;
    }

    // Change state to stop automatic queueing of samples
    m_DeviceStateChanged.SetState(DeviceState::DeviceStatePausing, S_OK, false);
    m_AudioClient->Stop();
    m_DeviceStateChanged.SetState(DeviceState::DeviceStatePaused, S_OK, true);
    return S_OK;
}

void WASAPIRenderer::RenderLoop()
{
    // timeBeginPeriod(1); // Increase clock granularity
    while (m_DeviceStateChanged.GetState() == DeviceState::DeviceStatePlaying) {
        WaitForSingleObject(m_SampleReadyEvent, 1);

        HRESULT hr = OnAudioSampleRequested(false);
        if (FAILED(hr) || hr == S_FALSE) {
            m_DeviceStateChanged.SetState(DeviceState::DeviceStateInError, hr, true);
            break;
        }
    }
}

void WASAPIRenderer::RenderLoopThreadMain(WASAPIRenderer* _this)
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    HANDLE mmcssHandle = NULL;
    DWORD mmcssTaskIndex = 0;
    // Ask MMCSS to temporarily boost the thread priority
    // to reduce glitches while the low-latency stream plays.
    // mmcssHandle = AvSetMmThreadCharacteristics(L"Audio", &mmcssTaskIndex);
    mmcssHandle = AvSetMmThreadCharacteristics(L"Audio", &mmcssTaskIndex);

    _this->RenderLoop();

    if (mmcssHandle) {
        AvRevertMmThreadCharacteristics(mmcssHandle);
    }
    CoUninitialize();
}

//
//  OnAudioSampleRequested()
//
//  Called when audio device fires m_SampleReadyEvent
//
HRESULT WASAPIRenderer::OnAudioSampleRequested(bool IsSilence)
{
    HRESULT hr = S_OK;
    UINT32 PaddingFrames = 0;
    UINT32 FramesAvailable = 0;

    // Get padding in existing buffer
    hr = m_AudioClient->GetCurrentPadding(&PaddingFrames);
    if (FAILED(hr)) {
        goto exit;
    }

    // Audio frames available in buffer

    // In non-HW shared mode, GetCurrentPadding represents the number of queued frames
    // so we can subtract that from the overall number of frames we have
    FramesAvailable = m_BufferFrames - PaddingFrames;

    // Only continue if we have buffer to write data
    if (FramesAvailable > 0) {
        if (IsSilence) {
            BYTE* Data;

            // Fill the buffer with silence
            hr = m_AudioRenderClient->GetBuffer(FramesAvailable, &Data);
            if (FAILED(hr)) {
                goto exit;
            }

            hr = m_AudioRenderClient->ReleaseBuffer(FramesAvailable, AUDCLNT_BUFFERFLAGS_SILENT);
            goto exit;
        }

        // Even if we cancel a work item, this may still fire due to the async
        // nature of things.  There should be a queued work item already to handle
        // the process of stopping or stopped
        if (m_DeviceStateChanged.GetState() == DeviceState::DeviceStatePlaying) {
            // Fill the buffer with a playback sample
            hr = GetToneSample(FramesAvailable);
        }
    }
exit:
    if (AUDCLNT_E_RESOURCES_INVALIDATED == hr) {
        m_DeviceStateChanged.SetState(DeviceState::DeviceStateUnInitialized, hr, false);
        m_AudioClient = nullptr;
        m_AudioRenderClient = nullptr;
        m_SampleReadyAsyncResult = nullptr;

        hr = InitializeAudioDevice();
    }

    return hr;
}

//
//  GetToneSample()
//
//  Fills buffer with a tone sample
//
HRESULT WASAPIRenderer::GetToneSample(UINT32 FramesAvailable)
{
    HRESULT hr = S_OK;
    BYTE* Data;

    // Post-Roll Silence
    if (m_audioSource->IsEOF()) {
        hr = m_AudioRenderClient->GetBuffer(FramesAvailable, &Data);
        if (SUCCEEDED(hr)) {
            // Ignore the return
            hr = m_AudioRenderClient->ReleaseBuffer(FramesAvailable, AUDCLNT_BUFFERFLAGS_SILENT);
        }

        hr = S_FALSE;

    } else {
        UINT32 ActualFramesToRead = m_audioSource->GetBufferLength() / m_MixFormat->nBlockAlign;
        UINT32 ActualBytesToRead = ActualFramesToRead * m_MixFormat->nBlockAlign;

        UINT32 batches = FramesAvailable / ActualFramesToRead;
        if (batches > 0) {
            hr = m_AudioRenderClient->GetBuffer(batches * ActualFramesToRead, &Data);
            if (SUCCEEDED(hr)) {
                for (UINT32 i = 0; i < batches && SUCCEEDED(hr); i++) {
                    hr = m_audioSource->FillSampleBuffer(ActualBytesToRead, Data);
                    Data += ActualBytesToRead;
                }
                if (SUCCEEDED(hr)) {
                    hr = m_AudioRenderClient->ReleaseBuffer(batches * ActualFramesToRead, 0);
                }
            }
        }
    }

    return hr;
}
