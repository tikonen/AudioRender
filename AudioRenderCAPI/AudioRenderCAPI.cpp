#include "include/AudioRenderCAPI.h"

#include <AudioDevice.hpp>
#include <AudioGraphics.hpp>
#include <DrawDevice.hpp>
#include <SimulatorView.hpp>

namespace
{
class IDeviceWrapper
{
public:
    virtual AudioRender::IDrawDevice* getDrawDevice() = 0;
    virtual ~IDeviceWrapper() = default;
};

class AudioRenderDeviceWrapper : public IDeviceWrapper
{
public:
    AudioRenderDeviceWrapper(float scaleX, float scaleY)
    {
        m_audioDevice.Initialize();
        m_audioGenerator = std::make_shared<AudioRender::AudioGraphicsBuilder>();

        m_audioGenerator->setScale(scaleX, scaleY);
        m_audioDevice.SetGenerator(m_audioGenerator);
        m_audioDevice.Start();
    }

    ~AudioRenderDeviceWrapper() override { m_audioDevice.Stop(); }

    AudioRender::IDrawDevice* getDrawDevice() override { return m_audioGenerator.get(); }

private:
    AudioRender::AudioDevice m_audioDevice;
    std::shared_ptr<AudioRender::AudioGraphicsBuilder> m_audioGenerator;
};

class ScreenRenderDeviceWrapper : public IDeviceWrapper
{
public:
    ScreenRenderDeviceWrapper(const char* backgroundImagePath, audioRender_Bool simulateFlicker, audioRender_Bool simulateBeamIdle)
        : m_running(true)
    {
        m_renderView = std::make_unique<SimulatorRenderView>(m_running);

        if (backgroundImagePath != nullptr) {
            m_renderView->loadBackground(backgroundImagePath);
        }

        m_renderView->start();
        m_renderView->SimulateFlicker(simulateFlicker);
        m_renderView->SimulateBeamIdle(simulateBeamIdle);
    }

    AudioRender::IDrawDevice* getDrawDevice() override { return m_renderView.get(); }

private:
    std::atomic_bool m_running;
    std::unique_ptr<SimulatorRenderView> m_renderView;
};

AudioRender::IDrawDevice* getDrawDevice(audioRender_DrawDevice* cApiDevice) { return static_cast<IDeviceWrapper*>(cApiDevice)->getDrawDevice(); }
}  // namespace

__declspec(dllexport) audioRender_DrawDevice* audioRender_DeviceInitAudioRender(float scaleX, float scaleY)
{
    return new AudioRenderDeviceWrapper(scaleX, scaleY);
}

__declspec(dllexport) audioRender_DrawDevice* audioRender_DeviceInitScreenRender(
    const char* backgroundImagePath, audioRender_Bool simulateFlicker, audioRender_Bool simulateBeamIdle)
{
    return new ScreenRenderDeviceWrapper(backgroundImagePath, simulateFlicker, simulateBeamIdle);
}

__declspec(dllexport) void audioRender_DeviceFree(audioRender_DrawDevice* device) { delete static_cast<IDeviceWrapper*>(device); }

__declspec(dllexport) bool audioRender_WaitSync(audioRender_DrawDevice* device)
{
    if (device == nullptr) return false;
    return getDrawDevice(device)->WaitSync();
}

__declspec(dllexport) void audioRender_Begin(audioRender_DrawDevice* device)
{
    if (device == nullptr) return;
    getDrawDevice(device)->Begin();
}

__declspec(dllexport) void audioRender_Submit(audioRender_DrawDevice* device)
{
    if (device == nullptr) return;
    getDrawDevice(device)->Submit();
}

__declspec(dllexport) struct audioRender_Rectangle audioRender_GetViewPort(audioRender_DrawDevice* device)
{
    if (device == nullptr) return {};
    AudioRender::Rectangle rectangle = getDrawDevice(device)->GetViewPort();
    audioRender_Rectangle cApiRectangle{};
    cApiRectangle.bottom = rectangle.bottom;
    cApiRectangle.left = rectangle.left;
    cApiRectangle.right = rectangle.right;
    cApiRectangle.top = rectangle.top;
    return cApiRectangle;
}

__declspec(dllexport) void audioRender_SetPoint(audioRender_DrawDevice* device, const struct audioRender_Point* p)
{
    if (device == nullptr) return;
    getDrawDevice(device)->SetPoint({p->x, p->y});
}

__declspec(dllexport) void audioRender_SetIntensity(audioRender_DrawDevice* device, float intensity)
{
    if (device == nullptr) return;
    getDrawDevice(device)->SetIntensity(intensity);
}

__declspec(dllexport) void audioRender_DrawCircle(audioRender_DrawDevice* device, float radius)
{
    if (device == nullptr) return;
    getDrawDevice(device)->DrawCircle(radius);
}

__declspec(dllexport) void audioRender_DrawLine(audioRender_DrawDevice* device, const struct audioRender_Point* to, float intensity)
{
    if (device == nullptr || to == nullptr) return;
    getDrawDevice(device)->DrawLine({to->x, to->y}, intensity);
}
