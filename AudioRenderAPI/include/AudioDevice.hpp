#pragma once

#include <atomic>
#include <mutex>

#include "DeviceState.hpp"
#include "IAudioGenerator.hpp"

namespace AudioRender
{
class AudioDevice
{
private:
    struct DeviceWrapper;

    DeviceWrapper* m_wrapper;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic<DeviceState> m_deviceState = DeviceState::DeviceStateUnInitialized;

public:
    AudioDevice();
    ~AudioDevice();

    void SetGenerator(std::shared_ptr<IAudioGenerator> generator);
    DeviceState GetDeviceState() { return m_deviceState; };

    struct Configuration {
        std::wstring deviceName = L"DAC Device";    // Name of preferred audio device. If not defined or found, try to use default devices as configured below.
        bool fallBackToCommunicationDevice = true;  // If true can use default communication audio device
        bool fallBackToDefaultDevice = true;        // If true can use default audio device
    };

    bool WaitForDeviceState(int seconds, DeviceState state);
    bool Initialize();
    bool InitializeWithConfig(Configuration& config);
    bool Start();
    void Stop();
};

}  // namespace AudioRender
