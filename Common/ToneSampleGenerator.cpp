//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

//
// ToneSampleGenerator.h
//

#include <Windows.h>

#include <mmreg.h>
#include <mfapi.h>

#include "ToneSampleGenerator.hpp"


const int TONE_DURATION_SEC = 10;
const double TONE_AMPLITUDE = 0.5;  // Scalar value, should be between 0.0 - 1.0

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

enum RenderSampleType {
    SampleTypeUnknown,
    SampleTypeFloat,
    SampleType16BitPCM,
};

//
//  CalculateMixFormatType()
//
//  Determine IEEE Float or PCM samples based on media type
//
inline RenderSampleType CalculateMixFormatType(WAVEFORMATEX* wfx)
{
    if ((wfx->wFormatTag == WAVE_FORMAT_PCM) ||
        ((wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) && (reinterpret_cast<WAVEFORMATEXTENSIBLE*>(wfx)->SubFormat == KSDATAFORMAT_SUBTYPE_PCM))) {
        if (wfx->wBitsPerSample == 16) {
            return RenderSampleType::SampleType16BitPCM;
        }
    } else if ((wfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
               ((wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) && (reinterpret_cast<WAVEFORMATEXTENSIBLE*>(wfx)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))) {
        return RenderSampleType::SampleTypeFloat;
    }

    return RenderSampleType::SampleTypeUnknown;
}

//
//  ToneSampleGenerator()
//
ToneSampleGenerator::ToneSampleGenerator(unsigned int frequency)
    : m_frequency(frequency)
    , m_sampleIdx(0)
{
}

//
//  ~ToneSampleGenerator()
//
ToneSampleGenerator::~ToneSampleGenerator()
{
    // Flush unused samples
    Flush();
}

//
//  GenerateSampleBuffer()
//
//  Create a linked list of sample buffers
//
HRESULT ToneSampleGenerator::Initialize(UINT32 FramesPerPeriod, WAVEFORMATEX* wfx)
{
    HRESULT hr = S_OK;

    UINT32 renderBufferSizeInBytes = FramesPerPeriod * wfx->nBlockAlign;
    UINT64 renderDataLength = (wfx->nSamplesPerSec * TONE_DURATION_SEC * wfx->nBlockAlign) + (renderBufferSizeInBytes - 1);
    UINT64 renderBufferCount = renderDataLength / renderBufferSizeInBytes;

    double theta = 0;

    for (UINT64 i = 0; i < renderBufferCount; i++) {
        std::vector<BYTE> sampleBuffer(renderBufferSizeInBytes);

        switch (CalculateMixFormatType(wfx)) {
            case RenderSampleType::SampleType16BitPCM:
                GenerateSineSamples<short>(sampleBuffer.data(), sampleBuffer.size(), m_frequency, wfx->nChannels, wfx->nSamplesPerSec, TONE_AMPLITUDE, &theta);
                break;

            case RenderSampleType::SampleTypeFloat:
                GenerateSineSamples<float>(sampleBuffer.data(), sampleBuffer.size(), m_frequency, wfx->nChannels, wfx->nSamplesPerSec, TONE_AMPLITUDE, &theta);
                break;

            default: return E_UNEXPECTED; break;
        }

        m_sampleQueue.emplace_back(sampleBuffer);
    }
    return hr;
}

//
// GenerateSineSamples()
//
//  Generate samples which represent a sine wave that fits into the specified buffer.
//
//  T:  Type of data holding the sample (short, int, byte, float)
//  Buffer - Buffer to hold the samples
//  BufferLength - Length of the buffer.
//  ChannelCount - Number of channels per audio frame.
//  SamplesPerSecond - Samples/Second for the output data.
//  InitialTheta - Initial theta value - start at 0, modified in this function.
//
template <typename T>
void ToneSampleGenerator::GenerateSineSamples(
    BYTE* Buffer, size_t BufferLength, DWORD Frequency, WORD ChannelCount, DWORD SamplesPerSecond, double Amplitude, double* InitialTheta)
{
    double sampleIncrement = (Frequency * (M_PI * 2)) / (double)SamplesPerSecond;
    T* dataBuffer = reinterpret_cast<T*>(Buffer);
    double theta = (InitialTheta != NULL ? *InitialTheta : 0);

    for (int i = 0; i < BufferLength / sizeof(T); i += ChannelCount) {
        double sinValue = Amplitude * sin(theta);
        for (int j = 0; j < ChannelCount; j++) {
            dataBuffer[i + j] = Convert<T>(sinValue) * (j ? -1 : 1);
        }
        theta += sampleIncrement;
    }

    if (InitialTheta != NULL) {
        *InitialTheta = theta;
    }
}

//
//  FillSampleBuffer()
//
//  File the Data buffer of size BytesToRead with the first item in the queue.  Caller is responsible for allocating and freeing buffer
//
HRESULT ToneSampleGenerator::FillSampleBuffer(UINT32 BytesToRead, BYTE* Data)
{
    if (nullptr == Data) {
        return E_POINTER;
    }

    std::vector<BYTE>& sample = m_sampleQueue[m_sampleIdx];

    if (BytesToRead > sample.size()) {
        return E_INVALIDARG;
    }

    memcpy(Data, sample.data(), BytesToRead);

    m_sampleIdx++;
    if (m_sampleIdx >= m_sampleQueue.size()) {
        m_sampleIdx = 0;
    }

    return S_OK;
}

//
//  Flush()
//
//  Remove and free unused samples from the queue
//
void ToneSampleGenerator::Flush()
{
    m_sampleIdx = 0;
    m_sampleQueue.clear();
}
