#pragma once
#include <Windows.h>

#include <ftprotocol.h>
#include "DrawDevice.hpp"

namespace AudioRender
{
class IntegratorGraphicsBuilder : public DrawDevice
{
public:    

    // how large range is used ]0, 1[, useful for flipping an axis or give the DAC and
    // integrator more headroom.
    void setScale(float xscale, float yscale)
    {
        m_xScale = xscale * 0.5f;
        m_yScale = yscale * 0.5f;
    }    

protected:
    // Graphics encoding to samples
    void EncodeSamples(const std::vector<GraphicsPrimitive>& ops);
    struct EncodeCtx {
        bool syncPoint;
    };
    int EncodeCircle(const GraphicsPrimitive& p, EncodeCtx& ctx);
    int EncodeLine(const GraphicsPrimitive& p, EncodeCtx& ctx);
    int EncodeSync(const GraphicsPrimitive& p, EncodeCtx& ctx);

    std::vector<FTSample> m_samples;
    
    // Amplitude scale
    float m_xScale = 0.5f;
    float m_yScale = 0.5f;
};

class IntegratorDevice : public IntegratorGraphicsBuilder
{
public:
    ~IntegratorDevice();

    bool Connect();
    void Disconnect();
    

    //==========================================================
    // IDrawDevice interface
    bool WaitSync() override;
    void Submit() override;

    // rounded to multiples of 5
    void SetFrameDuration(int ms);

    DWORD lastError() const { return m_lastError; }
    const char* lastErrorStr() const { return m_lastErrorStr.c_str(); }

protected:
    std::wstring findDeviceLink(const GUID& guid);    
    // Hardware handle
    HANDLE m_hdev = INVALID_HANDLE_VALUE;
    
    // wrapper to hide dependencies from the header
    struct WinUSBDevice;
    std::shared_ptr<WinUSBDevice> m_winusb;

    int m_frameDurationMs = 10;

    void clearError();
    void updateError(const char* str, DWORD err);
    DWORD m_lastError;
    std::string m_lastErrorStr;
};

}  // namespace AudioRender
