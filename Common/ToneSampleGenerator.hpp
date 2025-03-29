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

#include <vector>

#include "IAudioGenerator.hpp"

class ToneSampleGenerator : public IAudioGenerator
{
public:
    ToneSampleGenerator(unsigned int frequency);
    ~ToneSampleGenerator();

    //==========================================================
    // IAudioGenerator interface
    bool IsEOF() override { return (m_sampleQueue.size() == m_sampleIdx); };
    UINT32 GetBufferLength() override { return (m_sampleQueue.size() != 0 ? (UINT32)m_sampleQueue[0].size() : 0); };
    void Flush() override;

    HRESULT Initialize(UINT32 FramesPerPeriod, WAVEFORMATEX* wfx) override;
    HRESULT FillSampleBuffer(UINT32 BytesToRead, BYTE* Data) override;

private:
    template <typename T>
    void GenerateSineSamples(
        BYTE* Buffer, size_t BufferLength, DWORD Frequency, WORD ChannelCount, DWORD SamplesPerSecond, double Amplitude, double* InitialTheta);

    unsigned int m_frequency;

    std::vector<std::vector<BYTE>> m_sampleQueue;
    int m_sampleIdx;
};
