#pragma once

#include "DeviceState.hpp"

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

    bool WaitForDeviceState(int seconds, DeviceState state);
    bool Initialize();
    bool Start();
    void Stop();
};
