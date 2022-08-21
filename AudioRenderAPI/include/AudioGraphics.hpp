#pragma once

#include <vector>
#include <mutex>
#include <condition_variable>

#include "IAudioGenerator.hpp"
#include "DrawDevice.hpp"

namespace AudioRender
{
class AudioGraphicsBuilder : public IAudioGenerator, public DrawDevice
{
public:
    // how large range is used ]0, 1[, useful for flipping an axis or a with DAC that might need some margin due to biasing (it's 0 is not exactly 0 volts so
    // some clipping will happen on extreme values.)
    void setScale(float xscale, float yscale)
    {
        m_xScale = xscale;
        m_yScale = yscale;
    }

    //==========================================================
    // IDrawDevice interface
    bool WaitSync() override;
    void Submit() override;

    //==========================================================
    // IAudioGenerator interface
    bool IsEOF() override
    {
        // never stop the madness
        return false;
    }
    UINT32 GetBufferLength() override { return UINT32(m_audioBuffers[0].size()); }
    void Flush() override;

    HRESULT Initialize(UINT32 FramesPerPeriod, WAVEFORMATEX* wfx) override;
    HRESULT FillSampleBuffer(UINT32 BytesToRead, BYTE* Data) override;

private:
    // Graphics encoding to audio
    std::mutex m_mutex;
    void EncodeAudio(const std::vector<GraphicsPrimitive>& ops);
    struct EncodeCtx {
        int bufferIdx;
        int idx;
        bool syncPoint;
    };
    int EncodeCircle(const GraphicsPrimitive& p, EncodeCtx& ctx);
    int EncodeLine(const GraphicsPrimitive& p, EncodeCtx& ctx);
    int EncodeSync(const GraphicsPrimitive& p, EncodeCtx& ctx);
    bool AddToBuffer(float x, float y, EncodeCtx& ctx);
    std::vector<std::vector<BYTE>> m_audioBuffers;
    std::condition_variable m_frameCv;
    int m_minBufferCount;
    int m_maxBufferCount;
    int m_bufferIdx = 0;
    bool m_rendering = false;
    enum RenderSampleType {
        SampleTypeUnknown,
        SampleTypeFloat,
        SampleType16BitPCM,
    };
    void ResolveMixFormatType(WAVEFORMATEX* wfx);
    RenderSampleType m_sampleType = SampleTypeUnknown;

    WAVEFORMATEX m_wfx;
    // Amplitude scale
    float m_xScale = 1.0f;
    float m_yScale = 1.0f;
};

}  // namespace AudioRender
