#include "pch.h"

#include <assert.h>
#include <chrono>

#include "AudioGraphics.hpp"
#include <Log.hpp>
#include <mfapi.h>

#define QUEUE_WATERMARK 3

namespace AudioRender
{
// Generates steps to draw a box following edges
void buildFrameSteps(float steps[][2], int count)
{
    const float min = 0.0f;
    const float max = 1.0f;
    int s = 0;
    const float step = (max - min) / (count / 4);
    for (int i = 0; i < count / 4; i++, s++) {
        steps[s][0] = min;
        steps[s][1] = step * i + min;
        steps[s + count / 2][0] = max;
        steps[s + count / 2][1] = max - step * i + min;
    }
    for (int i = 0; i < count / 4; i++, s++) {
        steps[s][0] = step * i + min;
        steps[s][1] = max;
        steps[s + count / 2][0] = max - step * i + min;
        steps[s + count / 2][1] = min;
    }
}

#define FRAMESTEPCOUNT 64
static float idleFrameSteps[FRAMESTEPCOUNT][2];
static bool idleFrameStepsInitialized = false;

AudioGraphicsBuilder::AudioGraphicsBuilder()
    : m_readIdx(0)
    , m_writeIdx(0)
    , m_bufferCount(0)
    , m_bufferSize(0)
    , m_wfx{0}
{
    m_frameEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
    if (!idleFrameStepsInitialized) {
        buildFrameSteps(idleFrameSteps, FRAMESTEPCOUNT);
        idleFrameStepsInitialized = true;
    }
}

AudioGraphicsBuilder::~AudioGraphicsBuilder()
{
    if (m_frameEvent) CloseHandle(m_frameEvent);
}

bool AudioGraphicsBuilder::WaitSync(int timeout)
{
    if (m_writeIdx - m_readIdx > QUEUE_WATERMARK) {
        DWORD res = WaitForSingleObject(m_frameEvent, timeout ? timeout : INFINITE);

        if (res != WAIT_OBJECT_0) return false;
    }
    return true;
}

void AudioGraphicsBuilder::Submit() { EncodeAudio(m_operations); }

template <typename T>
inline T Convert(float Value);

template <>
inline float Convert<float>(float Value)
{
    return (Value);
};

template <>
inline short Convert<short>(float Value)
{
    return (short)roundf(Value * _I16_MAX);
};

template <>
inline int32_t Convert<int32_t>(float Value)
{
    return (int32_t)roundf(Value * _I32_MAX);
};

bool AudioGraphicsBuilder::AddToBuffer(float x, float y, EncodeCtx& ctx)
{
    void* buffer = m_audioBuffer.data() + m_bufferIdx;
    if (m_sampleType == RenderSampleType::SampleType16BitPCM) {
        short* pcmbuffer = static_cast<short*>(buffer);
        pcmbuffer[0] = Convert<short>(x);  // left channel
        pcmbuffer[1] = Convert<short>(y);  // right channel
    } else if (m_sampleType == RenderSampleType::SampleType24BitPCM) {
        uint8_t* pcmbuffer = static_cast<uint8_t*>(buffer);
        int32_t v;

        v = Convert<int32_t>(x) >> 8;
        pcmbuffer[0] = v & 0xFF;
        pcmbuffer[1] = (v >> 8) & 0xFF;
        pcmbuffer[2] = (v >> 16) & 0xFF;

        v = Convert<int32_t>(y) >> 8;
        pcmbuffer[3] = v & 0xFF;
        pcmbuffer[4] = (v >> 8) & 0xFF;
        pcmbuffer[5] = (v >> 16) & 0xFF;
    } else if (m_sampleType == RenderSampleType::SampleTypeFloat) {
        float* fltbuffer = static_cast<float*>(buffer);
        fltbuffer[0] = Convert<float>(x);  // left channel
        fltbuffer[1] = Convert<float>(y);  // right channel
    }
    m_bufferIdx += m_wfx.nBlockAlign;
    assert(m_bufferIdx <= m_bufferSize);

    if (m_bufferIdx >= m_bufferSize) {
        // audiorender buffer is full, submit it for rendering
        QueueBuffer();
    }
    return true;
}

void AudioGraphicsBuilder::QueueBuffer()
{
    m_bufferCount++;

    if (m_bufferIdx < m_bufferSize) {
        // zero out remaining bytes
        memset(m_audioBuffer.data() + m_bufferIdx, 0, m_bufferSize - m_bufferIdx);
    }

    // audiorender buffer is full, submit it for rendering
    m_renderBuffer[m_writeIdx % m_renderBuffer.size()] = m_audioBuffer;
    m_writeIdx++;
    m_bufferIdx = 0;
}

const float SpeedMultiplier = 5.0f;

int AudioGraphicsBuilder::EncodeCircle(const GraphicsPrimitive& p, EncodeCtx& ctx)
{
    const float CircleSegmentMultiplier = 50.0f;  // how many segments in unit circle
    const float Pi = 3.14159f;
    const int stepCount = lround(CircleSegmentMultiplier * p.r * p.intensity * SpeedMultiplier);
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
    int stepCount = lround(LineSegmentMultiplier * l * p.intensity * SpeedMultiplier + 0.5f);

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

// Keep beam out from center by drawing a box around screen
void AudioGraphicsBuilder::FillIdle(EncodeCtx& ctx)
{
    static unsigned int step = 0;

    while (m_bufferIdx < m_bufferSize && m_bufferIdx != 0) {
        float x = idleFrameSteps[step % FRAMESTEPCOUNT][0];
        float y = idleFrameSteps[step % FRAMESTEPCOUNT][1];
        step++;
        AddToBuffer(x, y, ctx);
    }
}

void AudioGraphicsBuilder::EncodeAudio(const std::vector<GraphicsPrimitive>& ops)
{
#if 0
    // Sawtooth debug signal
    EncodeCtx ctx{0};    

    // Steps can be calculated for a given frequency
    //int freq = 120;
    //int steps = m_wfx.nSamplesPerSec / freq / 2;

    int steps = 200; // 240 gives even buffer size for 100Hz @ 48000
    for (int i = 0; i < steps; i++) {
        AddToBuffer(0.8f * i / float(steps), 0.8f * i / float(steps), ctx);
    }
    for (int i = steps; i > 0; i--) {
        AddToBuffer(0.8f * i / float(steps), 0.8f * i / float(steps), ctx);
    }
#else
    int points = 0;
    EncodeCtx ctx{0};

    for (size_t i = 0; i < ops.size(); i++) {
        const GraphicsPrimitive& p = ops[i];
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
        if (m_bufferIdx > 0) {
            if (m_idleBox) {
                FillIdle(ctx);
            } else {
                // send partially completed buffer
                QueueBuffer();
            }
        }
    }
#endif
}

//  Determine IEEE Float or PCM samples based on media type
void AudioGraphicsBuilder::ResolveMixFormatType(WAVEFORMATEX* wfx)
{
    if ((wfx->wFormatTag == WAVE_FORMAT_PCM) ||
        ((wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) && (reinterpret_cast<WAVEFORMATEXTENSIBLE*>(wfx)->SubFormat == KSDATAFORMAT_SUBTYPE_PCM))) {
        if (wfx->wBitsPerSample == 16) {
            m_sampleType = SampleType16BitPCM;
        } else if (wfx->wBitsPerSample == 24) {
            m_sampleType = SampleType24BitPCM;
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
    int renderBufferSize = FramesPerPeriod * m_wfx.nBlockAlign;
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

    if (m_writeIdx - m_readIdx > 0) {
        const std::vector<uint8_t>& buffer = m_renderBuffer[m_readIdx++ % m_renderBuffer.size()];
        if (BytesToRead > buffer.size()) {
            return E_INVALIDARG;
        }
        assert(BytesToRead == m_bufferSize);
        memcpy(Data, buffer.data(), BytesToRead);

    } else {
        memset(Data, 0, BytesToRead);
    }

    // Notify sync if queue is running low.
    if (m_writeIdx - m_readIdx < QUEUE_WATERMARK) SetEvent(m_frameEvent);

    return S_OK;
}

void AudioGraphicsBuilder::Flush()
{
    m_readIdx = m_writeIdx = 0;
    m_bufferIdx = 0;
}

}  // namespace AudioRender
