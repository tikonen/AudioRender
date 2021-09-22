#include "pch.h"

#include <mmreg.h>
#include <mfapi.h>
#include <ppl.h>

#include "Log.hpp"

#include "WASAPIRenderer.hpp"
#include "IAudioGenerator.hpp"
#include "Audiodevice.hpp"

namespace AudioRender
{
// Wrapper hides the messy winrt dependencies from the class header
struct AudioDevice::DeviceWrapper {
    ComPtr<WASAPIRenderer> m_renderer;
};

AudioDevice::AudioDevice()
    : m_wrapper(new AudioDevice::DeviceWrapper())
{
}

AudioDevice::~AudioDevice() { delete m_wrapper; }


void AudioDevice::SetGenerator(std::shared_ptr<IAudioGenerator> generator) { m_wrapper->m_renderer->SetGenerator(generator); }

bool AudioDevice::WaitForDeviceState(int seconds, DeviceState state)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait_for(lock, std::chrono::seconds(seconds), [&] { return m_deviceState == state; });
    return m_deviceState == state;
};

bool AudioDevice::Initialize()
{
    winrt::init_apartment();
    // CoInitialize(NULL);
    HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);

    if (FAILED(hr)) {
        LOGE("MediaFoundation startup failed. %s.", HResultToString(hr));
    }

    bool rawSupported = false;
    winrt::hstring id = winrt::Windows::Media::Devices::MediaDevice::GetDefaultAudioRenderId(winrt::Windows::Media::Devices::AudioDeviceRole::Default);
    {
        std::vector<winrt::hstring> props;
        const winrt::hstring propName = winrt::to_hstring("System.Devices.AudioDevice.RawProcessingSupported");
        props.push_back(propName);
        winrt::Windows::Foundation::IAsyncOperation op = winrt::Windows::Devices::Enumeration::DeviceInformation::CreateFromIdAsync(id, std::move(props));
        auto deviceInfo = op.get();
        LOG("Device: %S", deviceInfo.Name().c_str());
        auto obj = deviceInfo.Properties().Lookup(propName);

        if (obj) {
            rawSupported = true;
            LOG("Raw audio supported");
        } else {
            rawSupported = false;
            LOGW("Raw audio not supported");
        }
    }

    hr = MakeAndInitialize<WASAPIRenderer>(&m_wrapper->m_renderer);
    if (FAILED(hr)) {
        LOGE("Unable to initialize renderer. %s", HResultToString(hr));
        return 1;
    }

    DEVICEPROPS props{0};

    props.IsLowLatency = true;
    props.IsHWOffload = false;
    props.IsBackground = false;
    props.IsRawChosen = true;
    props.IsRawSupported = rawSupported;

    m_wrapper->m_renderer->SetProperties(props);

    m_wrapper->m_renderer->GetDeviceStateEvent().addListener([&](DeviceState state, HRESULT hr) {
        LOG("State: %s Status: %s", DeviceStateChangedEvent::DeviceStateToStr(state), HResultToString(hr));
        m_deviceState = state;
        m_cv.notify_all();
    });
    return true;
}

bool AudioDevice::Start()
{
    m_wrapper->m_renderer->InitializeAudioDeviceAsync();

    if (WaitForDeviceState(5, DeviceState::DeviceStateInitialized)) {
        LOG("Audio latency %ld ms", std::lround(1000 * m_wrapper->m_renderer->GetPeriodInSeconds()));

        m_wrapper->m_renderer->StartPlaybackAsync();

        if (WaitForDeviceState(2, DeviceState::DeviceStatePlaying)) {
            return true;
        }
    }
    return false;
}

void AudioDevice::Stop()
{
    m_wrapper->m_renderer->StopPlaybackAsync();
    WaitForDeviceState(2, DeviceState::DeviceStateStopped);

    MFShutdown();
}

}  // namespace AudioRender
