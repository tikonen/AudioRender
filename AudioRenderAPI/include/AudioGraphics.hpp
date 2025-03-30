#pragma once

#include <vector>
#include <queue>
#include <mutex>
#include <array>
#include <condition_variable>

#include "IAudioGenerator.hpp"
#include "DrawDevice.hpp"

namespace AudioRender
{
class AudioGraphicsBuilder : public IAudioGenerator, public DrawDevice
{
public:
    AudioGraphicsBuilder();
    ~AudioGraphicsBuilder();

    // how large range is used ]0, 1[, useful for flipping an axis or a with DAC that might need some margin due to biasing (it's 0 is not exactly 0 volts so
    // some clipping will happen on extreme values.)
    void setScale(float xscale, float yscale)
    {
        m_xScale = xscale;
        m_yScale = yscale;
    }

    void setFixedRenderingRate(bool fixedRate) { m_fixedRate = fixedRate; }
    void setIdleBox(bool idleBox) { m_idleBox = idleBox; }

    //==========================================================
    // IDrawDevice interface
    bool WaitSync(int timeout) override;
    void Submit() override;

    //==========================================================
    // IAudioGenerator interface
    bool IsEOF() override
    {
        // never stop the madness
        return false;
    }
    UINT32 GetBufferLength() override { return UINT32(m_bufferSize); }
    void Flush() override;

    HRESULT Initialize(UINT32 FramesPerPeriod, WAVEFORMATEX* wfx) override;
    HRESULT FillSampleBuffer(UINT32 BytesToRead, BYTE* Data) override;

private:
    // Graphics encoding to audio
    void EncodeAudio(const std::vector<GraphicsPrimitive>& ops);
    struct EncodeCtx {
        bool syncPoint;
    };
    int EncodeCircle(const GraphicsPrimitive& p, EncodeCtx& ctx);
    int EncodeLine(const GraphicsPrimitive& p, EncodeCtx& ctx);
    int EncodeSync(const GraphicsPrimitive& p, EncodeCtx& ctx);
    bool AddToBuffer(float x, float y, EncodeCtx& ctx);
    void QueueBuffer();
    void FillIdle(EncodeCtx& ctx);

    // Current buffer that is used to build rendering data
    std::vector<uint8_t> m_audioBuffer;
    int m_bufferIdx = 0;
    int m_bufferSize;
    bool m_fixedRate = false;
    bool m_idleBox = false;

    // Buffers that are ready for rendering and can be picked up by the FillSampleBuffer
    std::array<std::vector<uint8_t>, 128> m_renderBuffer;
    uint32_t m_readIdx;
    uint32_t m_writeIdx;
    HANDLE m_frameEvent;
    uint32_t m_bufferCount;

    enum RenderSampleType {
        SampleTypeUnknown,
        SampleTypeFloat,
        SampleType16BitPCM,
        SampleType24BitPCM,
    };
    void ResolveMixFormatType(WAVEFORMATEX* wfx);
    RenderSampleType m_sampleType = SampleTypeUnknown;

    WAVEFORMATEX m_wfx;
    // Amplitude scale
    float m_xScale = 1.0f;
    float m_yScale = 1.0f;
};

}  // namespace AudioRender
