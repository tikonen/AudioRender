#include "pch.h"

#include <assert.h>

#include "AudioGraphics.hpp"
#include <Log.hpp>
#include <mfapi.h>

namespace AudioRender
{
bool AudioGraphicsBuilder::WaitSync()
{
    // Consider sync completed when there is one or less ready data block waiting for rendering
    std::unique_lock<std::mutex> lock(m_renderMutex);
    m_frameCv.wait(lock, [&]() { return m_renderQueue.size() <= 1; });
    return true;
}

void AudioGraphicsBuilder::Submit() { EncodeAudio(m_operations); }

template <typename T>
T Convert(float Value);

template <>
float Convert<float>(float Value)
{
    return (Value);
};

template <>
short Convert<short>(float Value)
{
    return (short)(Value * _I16_MAX);
};

bool AudioGraphicsBuilder::AddToBuffer(float x, float y, EncodeCtx& ctx)
{
    void* buffer = m_audioBuffer.data() + m_bufferIdx;
    if (m_sampleType == RenderSampleType::SampleType16BitPCM) {
        short* pcmbuffer = static_cast<short*>(buffer);
        pcmbuffer[0] = Convert<short>(x);  // left channel
        pcmbuffer[1] = Convert<short>(y);  // right channel
        m_bufferIdx += sizeof(short) * 2;

    } else if (m_sampleType == RenderSampleType::SampleTypeFloat) {
        float* fltbuffer = static_cast<float*>(buffer);
        fltbuffer[0] = Convert<float>(x);  // left channel
        fltbuffer[1] = Convert<float>(y);  // right channel
        m_bufferIdx += sizeof(float) * 2;
    }
    assert(m_bufferIdx <= m_bufferSize);

    if (m_bufferIdx >= m_bufferSize) {
        // audiorender buffer is full, submit it for rendering
        QueueBuffer();
    }
    return true;
}

void AudioGraphicsBuilder::QueueBuffer()
{
    if (m_bufferIdx < m_bufferSize) {
        // zero out remaining bytes
        memset(m_audioBuffer.data() + m_bufferIdx, 0, m_bufferSize - m_bufferIdx);
    }

    // audiorender buffer is full, submit it for rendering
    std::lock_guard<std::mutex> lock(m_renderMutex);
    m_renderQueue.emplace(std::move(m_audioBuffer));
    m_audioBuffer = std::vector<uint8_t>(m_bufferSize);
    m_bufferIdx = 0;
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
    const float l = sqrtf(vx * vx + vy * vy);
    int stepCount = std::lround(LineSegmentMultiplier * l * p.intensity * SpeedMultiplier + 0.5f);

    // If syncpoint has not been set don't draw the first dot as it was drawn already on previous
    // encode call.
    int startPoint = ctx.syncPoint ? 0 : 1;

    for (int i = startPoint; i <= stepCount; i++) {
        float x = p.p.x + i * vx / stepCount;
        float y = p.p.y + i * vy / stepCount;
        AddToBuffer(x * m_xScale, y * m_yScale, ctx);
    }
    ctx.syncPoint = false;
    return stepCount - startPoint;
}

int AudioGraphicsBuilder::EncodeSync(const GraphicsPrimitive& p, EncodeCtx& ctx)
{
    AddToBuffer(0, 0, ctx);
    ctx.syncPoint = true;
    return 1;
}

void AudioGraphicsBuilder::EncodeAudio(const std::vector<GraphicsPrimitive>& ops)
{
    int points = 0;
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

    if (m_fixedRate) {
        // This mode submits always buffers for rendering, even when there is
        // not enough data in the buffer. This limits rendering speed.
        if (m_bufferIdx < m_bufferSize) {
            QueueBuffer();
        }
    }
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

    // Calculate effective fps
    int fps = m_wfx.nSamplesPerSec / renderBufferCount / FramesPerPeriod;

    m_bufferSize = renderBufferSize;
    m_audioBuffer.resize(m_bufferSize);

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

    std::lock_guard<std::mutex> lock(m_renderMutex);

    if (m_renderQueue.size() > 0) {
        const auto& buffer = m_renderQueue.front();
        if (BytesToRead > buffer.size()) {
            return E_INVALIDARG;
        }
        assert(BytesToRead == buffer.size());
        memcpy(Data, buffer.data(), BytesToRead);
        m_renderQueue.pop();
    }

    // Notify sync if queue is running low.
    if (m_renderQueue.size() <= 1) m_frameCv.notify_all();

    return S_OK;
}

void AudioGraphicsBuilder::Flush()
{
    std::lock_guard<std::mutex> lock(m_renderMutex);
    while (m_renderQueue.size()) m_renderQueue.pop();
    m_bufferIdx = 0;
}

}  // namespace AudioRender
