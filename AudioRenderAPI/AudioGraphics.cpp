#include "pch.h"

#include <assert.h>

#include "AudioGraphics.hpp"
#include <Log.hpp>

namespace AudioRender
{
bool AudioGraphicsBuilder::WaitSync()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_frameCv.wait(lock, [&]() { return m_rendering == false; });
    return true;
}

void AudioGraphicsBuilder::Submit()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    EncodeAudio(m_operations);
    m_bufferIdx = 0;
}

#include <mmreg.h>
#include <mfapi.h>

template <typename T>
T Convert(double Value);

//
//  Convert from double to float, byte, short or int32.
//
template <>
float Convert<float>(double Value)
{
    return (float)(Value);
};

template <>
short Convert<short>(double Value)
{
    return (short)(Value * _I16_MAX);
};

bool AudioGraphicsBuilder::AddToBuffer(float x, float y, EncodeCtx& ctx)
{
    while (true) {
        // figure out where to write data
        if (ctx.bufferIdx >= m_maxBufferCount) return false;
        if (ctx.bufferIdx >= m_audioBuffers.size()) {
            m_audioBuffers.resize(ctx.bufferIdx + 1);
            m_audioBuffers[ctx.bufferIdx].resize(m_audioBuffers[0].size());
        }
        if (ctx.idx >= m_audioBuffers[ctx.bufferIdx].size()) {
            ctx.idx = 0;
            ctx.bufferIdx++;
            continue;
        }
        break;
    }
    assert(ctx.bufferIdx < m_audioBuffers.size() && m_audioBuffers.size() <= m_maxBufferCount);

    void* buffer = m_audioBuffers[ctx.bufferIdx].data() + ctx.idx;
    if (m_sampleType == RenderSampleType::SampleType16BitPCM) {
        short* pcmbuffer = static_cast<short*>(buffer);
        pcmbuffer[0] = Convert<short>(x);  // left channel
        pcmbuffer[1] = Convert<short>(y);  // right channel
        ctx.idx += sizeof(short) * 2;

    } else if (m_sampleType == RenderSampleType::SampleTypeFloat) {
        float* fltbuffer = static_cast<float*>(buffer);
        fltbuffer[0] = Convert<float>(x);  // left channel
        fltbuffer[1] = Convert<float>(y);  // right channel
        ctx.idx += sizeof(float) * 2;
    }
    assert(ctx.idx <= m_audioBuffers[ctx.bufferIdx].size());

    return true;
}

const float SpeedMultiplier = 5.0f;

int AudioGraphicsBuilder::EncodeCircle(const GraphicsPrimitive& p, EncodeCtx& ctx)
{
    const float CircleSegmentMultiplier = 50.0f;  // how many segments in unit circle
    const float Pi = 3.14159f;
    const int stepCount = std::lround(CircleSegmentMultiplier * p.r * p.intensity * SpeedMultiplier);
    const float angleStep = Pi * 2 / stepCount;

    for (int i = 0; i < stepCount + 1; i++) {
        float x = p.r * sinf(i * angleStep) + p.p.x;
        float y = p.r * cosf(i * angleStep) + p.p.y;
        AddToBuffer(x * m_xScale, y * m_yScale, ctx);
    }
    return stepCount;
}

int AudioGraphicsBuilder::EncodeLine(const GraphicsPrimitive& p, EncodeCtx& ctx)
{
    const float LineSegmentMultiplier = 12.0f;  // how many segments in a unit line
    // get line length
    const float vx = p.toPoint.x - p.p.x;
    const float vy = p.toPoint.y - p.p.y;
    const float l = sqrtf(pow(vx, 2) + pow(vy, 2));
    int stepCount = std::lround(LineSegmentMultiplier * l * p.intensity * SpeedMultiplier + 0.5f);

    // TODO grading intensity

    for (int i = 0; i <= stepCount; i++) {
        float x = p.p.x + i * vx / stepCount;
        float y = p.p.y + i * vy / stepCount;
        AddToBuffer(x * m_xScale, y * m_yScale, ctx);
    }
    return stepCount;
}

int AudioGraphicsBuilder::EncodeSync(const GraphicsPrimitive& p, EncodeCtx& ctx)
{
    const int stepCount = 2;  // how many slots stabilisation takes

    for (int i = 0; i < stepCount; i++) {
        float x = p.p.x;
        float y = p.p.y;
        AddToBuffer(x * m_xScale, y * m_yScale, ctx);
    }
    return stepCount;
}

void AudioGraphicsBuilder::EncodeAudio(const std::vector<GraphicsPrimitive>& ops)
{
    // const unsigned int pointCount = (unsigned int)(m_audioBuffers.size() * m_audioBuffers[0].size() / m_wfx.nBlockAlign);
    unsigned int points = 0;

    EncodeCtx ctx{0};

    for (size_t i = 0; i < m_operations.size(); i++) {
        const GraphicsPrimitive& p = m_operations[i];
        switch (p.type) {
            case GraphicsPrimitive::Type::DRAW_CIRCLE: points += EncodeCircle(p, ctx); break;
            case GraphicsPrimitive::Type::DRAW_LINE: points += EncodeLine(p, ctx); break;
            case GraphicsPrimitive::Type::DRAW_SYNC: points += EncodeSync(p, ctx); break;
            default:
                // Unknown
                break;
        }
    }

    // zero out rest
    if (ctx.bufferIdx < m_minBufferCount) {
        m_audioBuffers.resize(m_minBufferCount);
    }
    for (size_t i = ctx.bufferIdx; i < m_audioBuffers.size(); i++) {
        auto& buf = m_audioBuffers[i];
        if (ctx.idx < buf.size()) {
            memset(buf.data() + ctx.idx, 0, buf.size() - ctx.idx);
        }
    }
    m_rendering = true;
    // TODO compare points to pointCount
    // printf("Buf: %d Idx: %d\n", ctx.bufferIdx, ctx.idx);
}

//  Determine IEEE Float or PCM samples based on media type
void AudioGraphicsBuilder::ResolveMixFormatType(WAVEFORMATEX* wfx)
{
    if ((wfx->wFormatTag == WAVE_FORMAT_PCM) ||
        ((wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) && (reinterpret_cast<WAVEFORMATEXTENSIBLE*>(wfx)->SubFormat == KSDATAFORMAT_SUBTYPE_PCM))) {
        if (wfx->wBitsPerSample == 16) {
            m_sampleType = SampleType16BitPCM;
        }
    } else if ((wfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
               ((wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) && (reinterpret_cast<WAVEFORMATEXTENSIBLE*>(wfx)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))) {
        m_sampleType = RenderSampleType::SampleTypeFloat;
    } else {
        m_sampleType = RenderSampleType::SampleTypeUnknown;
    }
}

HRESULT AudioGraphicsBuilder::Initialize(UINT32 FramesPerPeriod, WAVEFORMATEX* wfx)
{
    HRESULT hr = S_OK;

    m_wfx = *wfx;

    if (m_wfx.nChannels != 2) {
        // must be stereo to encode X and Y
        return E_NOTIMPL;
    }
    const unsigned int Frequency = 50;

    // Calculate buffer size and number of buffers needed for this frequency (framerate) and samplerate
    int renderBufferSize = FramesPerPeriod * m_wfx.nBlockAlign;
    long renderDataLength = (m_wfx.nSamplesPerSec / Frequency * m_wfx.nBlockAlign) + (renderBufferSize - 1);
    int renderBufferCount = renderDataLength / renderBufferSize;

    m_minBufferCount = renderBufferCount;
    m_maxBufferCount = 2 * renderBufferCount;

    // Calculate effective fps
    int fps = m_wfx.nSamplesPerSec / m_minBufferCount / FramesPerPeriod;

    m_audioBuffers.resize(m_minBufferCount);
    for (size_t i = 0; i < m_audioBuffers.size(); i++) {
        m_audioBuffers[i].resize(renderBufferSize);
    }

    ResolveMixFormatType(wfx);
    if (m_sampleType == RenderSampleType::SampleTypeUnknown) {
        hr = E_INVALIDARG;
    }

    return hr;
}

HRESULT AudioGraphicsBuilder::FillSampleBuffer(UINT32 BytesToRead, BYTE* Data)
{
    if (nullptr == Data) {
        return E_POINTER;
    }
    m_rendering = true;
    std::lock_guard<std::mutex> lock(m_mutex);

    int idx = m_bufferIdx++;
    if (m_bufferIdx == m_audioBuffers.size()) {
        // this frame data has been dispatched
        m_bufferIdx = 0;
        m_rendering = false;
        m_frameCv.notify_all();
    }
    const auto& buffer = m_audioBuffers[idx];
    if (BytesToRead > buffer.size()) {
        return E_INVALIDARG;
    }
    memcpy(Data, buffer.data(), BytesToRead);

    return S_OK;
}

void AudioGraphicsBuilder::Flush()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for (size_t i = 0; i < m_audioBuffers.size(); i++) {
        memset(m_audioBuffers[i].data(), 0, m_audioBuffers[i].size());
    }
    m_bufferIdx = 0;
}

}  // namespace AudioRender
