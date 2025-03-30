#pragma once

#include <memory>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <condition_variable>

#include <imgui.h>

#include <d3d10_1.h>
#include <d3d10.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <tchar.h>

#include <SmartPtr.hpp>

#include "DrawDevice.hpp"

class SimulatorRenderView : public AudioRender::DrawDevice
{
public:
    SimulatorRenderView(std::atomic_bool& running);
    ~SimulatorRenderView();

    bool start();

    bool loadBackground(const char* image);

    // When there is nothing to render the beam will not move and returns to center of the screen.
    void SimulateBeamIdle(bool enabled) { m_idleBeam = enabled; }

    // If there is lot to render the flicker has tendency to increase.
    void SimulateFlicker(bool enabled) { m_flicker = enabled; }

    //==========================================================
    // IDrawDevice interface
    bool WaitSync(int timeoutms) override;
    void Submit() override;

private:
    // Synchronizes two threads at the same point
    class SyncPoint
    {
    public:
        void sync(std::unique_lock<std::mutex>& lock)
        {
            waitSync++;
            if (waitSync == 2) {
                // another thread is already waiting, wake it up
                cv.notify_one();
            } else if (!closed) {
                // another thread was not there yet, wait for it
                cv.wait(lock);
            }
            waitSync--;
        }

        void close()
        {
            closed = true;
            cv.notify_all();
        }

    private:
        std::condition_variable cv;
        int waitSync = 0;
        bool closed = false;
    };


    void loadSettings();
    void saveSettings();

    unsigned char* m_imageData = NULL;
    int m_imageWidth;
    int m_imageHeight;

    int m_drawWidth = 274;
    int m_drawHeight = 272;
    int m_drawXOffset = 4;
    int m_drawYOffset = -14;
    bool m_flicker = false;
    bool m_idleBeam = true;
    SyncPoint m_frameSyncPoint;
    std::condition_variable m_frameSubmitCv;
    std::mutex m_mutex;

    void createRenderTarget();
    void cleanupRenderTarget();
    HRESULT createDeviceD3D(HWND hWnd);
    void cleanupDeviceD3D();
    ImTextureID m_textureId = nullptr;

    void WinMainProc();
    static LRESULT WINAPI PlotterWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    SmartPtr<ID3D10Device> m_pd3dDevice;
    SmartPtr<IDXGISwapChain> m_pSwapChain;
    SmartPtr<ID3D10RenderTargetView> m_mainRenderTargetView;
    SmartPtr<ID3D10Texture2D> m_texture;
    SmartPtr<ID3D10ShaderResourceView> m_resourceView;

    std::thread m_windowThread;
    int m_width;
    int m_height;
    std::atomic_bool& m_running;
};
