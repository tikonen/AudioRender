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

#pragma once

#define _USE_MATH_DEFINES
#include <math.h>
#include <limits.h>

#include "IAudioGenerator.hpp"

class ToneSampleGenerator : public IAudioGenerator
{
public:
    ToneSampleGenerator(unsigned int frequency);
    ~ToneSampleGenerator();

    //==========================================================
    // IAudioGenerator interface
    bool IsEOF() override { return (m_SampleQueue == nullptr); };
    UINT32 GetBufferLength() override { return (m_SampleQueue != nullptr ? m_SampleQueue->BufferSize : 0); };
    void Flush() override;

    HRESULT Initialize(UINT32 FramesPerPeriod, WAVEFORMATEX* wfx) override;
    HRESULT FillSampleBuffer(UINT32 BytesToRead, BYTE* Data) override;

private:
    template <typename T>
    void GenerateSineSamples(
        BYTE* Buffer, size_t BufferLength, DWORD Frequency, WORD ChannelCount, DWORD SamplesPerSecond, double Amplitude, double* InitialTheta);

    unsigned int m_frequency;

    struct RenderBuffer {
        UINT32 BufferSize;
        UINT32 BytesFilled;
        BYTE* Buffer;
        RenderBuffer* Next;

        RenderBuffer()
            : BufferSize(0)
            , BytesFilled(0)
            , Buffer(nullptr)
            , Next(nullptr)
        {
        }

        ~RenderBuffer()
        {
            delete Buffer;
            Buffer = nullptr;
        }
    };

    RenderBuffer* m_SampleQueue;
    RenderBuffer** m_SampleQueueTail;
};
