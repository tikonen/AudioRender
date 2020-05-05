#pragma once


class IAudioGenerator
{
public:
    virtual ~IAudioGenerator() = default;

    virtual bool IsEOF() = 0;
    virtual UINT32 GetBufferLength() = 0;
    virtual void Flush() = 0;

    virtual HRESULT Initialize(UINT32 FramesPerPeriod, WAVEFORMATEX* wfx) = 0;
    virtual HRESULT FillSampleBuffer(UINT32 BytesToRead, BYTE* Data) = 0;
};
