#pragma once

#include <Windows.h>
#include <functional>

enum class DeviceState {
    DeviceStateUnInitialized,
    DeviceStateInError,
    DeviceStateDiscontinuity,
    DeviceStateFlushing,
    DeviceStateActivated,
    DeviceStateInitialized,
    DeviceStateStarting,
    DeviceStatePlaying,
    DeviceStateCapturing,
    DeviceStatePausing,
    DeviceStatePaused,
    DeviceStateStopping,
    DeviceStateStopped
};


class DeviceStateChangedEvent
{
public:
    DeviceStateChangedEvent()
        : m_state(DeviceState::DeviceStateUnInitialized)
    {
    }

    DeviceState GetState() { return m_state; };

    void SetState(DeviceState newState, HRESULT hr, bool fireEvent)
    {
        if (m_state != newState) {
            m_state = newState;

            if (fireEvent) {
                for (auto& cb : m_listeners) {
                    cb(newState, hr);
                }
            }
        }
    }

    static constexpr const char* DeviceStateToStr(DeviceState state)
    {
#define _DS_CASE(s) \
    case DeviceState::s: return #s

        switch (state) {
            _DS_CASE(DeviceStateUnInitialized);
            _DS_CASE(DeviceStateInError);
            _DS_CASE(DeviceStateDiscontinuity);
            _DS_CASE(DeviceStateFlushing);
            _DS_CASE(DeviceStateActivated);
            _DS_CASE(DeviceStateInitialized);
            _DS_CASE(DeviceStateStarting);
            _DS_CASE(DeviceStatePlaying);
            _DS_CASE(DeviceStateCapturing);
            _DS_CASE(DeviceStatePausing);
            _DS_CASE(DeviceStatePaused);
            _DS_CASE(DeviceStateStopping);
            _DS_CASE(DeviceStateStopped);
        }
        return "Unknown";
#undef _DS_CASE
    }

    void addListener(std::function<void(DeviceState, HRESULT)> listener) { m_listeners.emplace_back(listener); }

private:
    std::vector<std::function<void(DeviceState, HRESULT)>> m_listeners;
    DeviceState m_state;
};
