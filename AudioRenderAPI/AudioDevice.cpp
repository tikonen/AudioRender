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

// Argument op should be IAsyncAction or IAsyncOperation
template <class T>
void WaitSyncOp(T&& op)
{
    // https://docs.microsoft.com/en-us/archive/msdn-magazine/2018/june/c-effective-async-with-coroutines-and-c-winrt
    HANDLE event = CreateEvent(nullptr, true, false, nullptr);
    op.Completed([event](auto&& async, winrt::Windows::Foundation::AsyncStatus status) { SetEvent(event); });
    WaitForSingleObject(event, INFINITE);
    CloseHandle(event);
}


bool AudioDevice::Initialize()
{
    // Initializing single threaded to allow embedding as Unity plugin
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);

    if (FAILED(hr)) {
        LOGE("MediaFoundation startup failed. %s.", HResultToString(hr));
    }

    // List all audio devices and attempt to use DAC Driver device if it's available. If not, use default communications audio device and
    // failing that the default audio device.
    winrt::hstring id = MediaDevice::GetDefaultAudioRenderId(winrt::Windows::Media::Devices::AudioDeviceRole::Communications);
    if (id.empty()) {
        id = MediaDevice::GetDefaultAudioRenderId(winrt::Windows::Media::Devices::AudioDeviceRole::Default);
    }
    bool rawSupported = false;
    {
        // Prefer DAC device if it's found
        const winrt::hstring dacDeviceName = winrt::to_hstring("DAC Device");

        auto op = winrt::Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(winrt::Windows::Devices::Enumeration::DeviceClass::AudioRender);
        WaitSyncOp(op);
        auto devices = op.GetResults();

        for (auto& deviceInfo : devices) {
            auto name = deviceInfo.Name();
            LOG("Device: %S", name.c_str());
            auto it = std::search(name.begin(), name.end(), dacDeviceName.begin(), dacDeviceName.end());
            if (it != name.end()) {
                id = deviceInfo.Id();
            }
        }

        std::vector<winrt::hstring> props;
        const winrt::hstring propName = winrt::to_hstring("System.Devices.AudioDevice.RawProcessingSupported");
        props.push_back(propName);
        winrt::Windows::Foundation::IAsyncOperation opInfo = winrt::Windows::Devices::Enumeration::DeviceInformation::CreateFromIdAsync(id, std::move(props));

        WaitSyncOp(opInfo);

        auto deviceInfo = opInfo.GetResults();
        auto obj = deviceInfo.Properties().Lookup(propName);

        LOG("Using Device: %S", deviceInfo.Name().c_str());
        if (obj) {
            rawSupported = true;
            LOG("Raw audio supported");
        } else {
            rawSupported = false;
            LOGW("Raw audio not supported");
        }
        /*
        for (auto& p : deviceInfo.Properties()) {
            LOG("%S\n", p.Key().c_str());
        }
        */
    }

    hr = MakeAndInitialize<WASAPIRenderer>(&m_wrapper->m_renderer);
    m_wrapper->m_renderer->SetDeviceId(id);
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
    HRESULT hr = S_OK;
    hr = m_wrapper->m_renderer->InitializeAudioDeviceAsync();

    if (SUCCEEDED(hr) && WaitForDeviceState(5, DeviceState::DeviceStateInitialized)) {
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
