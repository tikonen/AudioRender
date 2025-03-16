#include "pch.h"
#include <initguid.h>
#include <SetupAPI.h>
#include <winusb.h>
#include <strsafe.h>

#include <vector>

#include "IntegratorDevice.hpp"

//#pragma comment(lib, "winusb.lib")
//#pragma comment(lib, "setupapi.lib")

// {b436698d-63c5-4c12-9a20-f8750d5060c6}
DEFINE_GUID(GUID_DEVINTERFACE_FAKETREX, 0xb436698d, 0x63c5, 0x4c12, 0x9a, 0x20, 0xf8, 0x75, 0x0d, 0x50, 0x60, 0xc6);

namespace AudioRender
{
#define INTEG_R 10e3f
#define INTEG_C 47e-9f
#define INTEG_STABILIZATION_TIME_US 50
#define MAX_PACKETS_PER_FRAME 6

#define SPEED_SCALE 5000 

static float norm(float x0, float y0, float x1, float y1) { return sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0)); }

// Vo = -Vd * t / RC
static float vOutEst(float Vd, float t) { return -Vd * t / (INTEG_C * INTEG_R); }

// Vd = -RC * Vo / t
static float vDeltaEst(float Vo, float t) { return (INTEG_C * INTEG_R * -Vo) / t; }

#define INTEG_DAC_MAX ((1U << 12) - 1)
#define INTEG_DAC_REF (INTEG_DAC_MAX / 2)

// map [-0.5, 0.5] => [0,dacmax]
static unsigned int dacMap(float c)
{
    unsigned int v = static_cast<unsigned int>(roundf((c + 0.5f) * INTEG_DAC_MAX));
    return std::clamp(v, 0U, INTEG_DAC_MAX);
}

static void setFramePacketSize(FTPacket* packet, int samplec)
{
    packet->frame.count = samplec;
    packet->size = offsetof(FTPacket, frame.samples) + sizeof(FTSample) * packet->frame.count;
}

static void pointSample(FTSample& sample, float x0, float y0)
{
    sample.dx = 0;
    sample.x = dacMap(x0);
    sample.dy = 0;
    sample.y = dacMap(y0);
    sample.wait = INTEG_STABILIZATION_TIME_US;  // stabilization time
}

static void pathSample(FTSample& sample, float x0, float y0, float x1, float y1, float speed)
{
    const float t = SPEED_SCALE * norm(x0, y0, x1, y1) * 1e-6f * speed;
    const float Vdx = vDeltaEst(x1 - x0, t);
    const float Vdy = vDeltaEst(y1 - y0, t);

    sample.dx = 1;
    sample.x = dacMap(x0 + Vdx);
    sample.dy = 1;
    sample.y = dacMap(y0 + Vdy);
    sample.wait = (uint16_t)(t * 1e6);  // TODO check max value
}

void IntegratorGraphicsBuilder::EncodeSamples(const std::vector<GraphicsPrimitive>& ops)
{
    int points = 0;
    EncodeCtx ctx{0};

    m_samples.clear();

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
}

int IntegratorGraphicsBuilder::EncodeSync(const GraphicsPrimitive& p, EncodeCtx& ctx)
{
    FTSample sample;
    pointSample(sample, p.p.x * m_xScale, p.p.y * m_yScale);

    m_samples.emplace_back(sample);
    ctx.syncPoint = true;

    return 1;
}

int IntegratorGraphicsBuilder::EncodeLine(const GraphicsPrimitive& p, EncodeCtx& ctx)
{
    const int MaxTimeus = (1 << 10) - 1;  // us

    FTSample sample;
    pathSample(sample, m_xScale * p.p.x, m_yScale * p.p.y, m_xScale * p.toPoint.x, m_yScale * p.toPoint.y, p.intensity);

    m_samples.emplace_back(sample);

    ctx.syncPoint = false;

    return 1;
}

int IntegratorGraphicsBuilder::EncodeCircle(const GraphicsPrimitive& p, EncodeCtx& ctx)
{
    const float CircleSegmentMultiplier = 50.0f;  // how many segments in unit circle
    const float Pi = 3.14159f;
    const int stepCount = (int)std::ceil(CircleSegmentMultiplier * p.r * p.intensity);
    const float angleStep = Pi * 2 / stepCount;

    Point prev;
    for (int i = 0; i < stepCount + 1; i++) {
        float x = p.r * sinf(i * angleStep) + p.p.x;
        float y = p.r * cosf(i * angleStep) + p.p.y;        
        FTSample sample;
        if (i == 0) {
            pointSample(sample, x * m_xScale, y * m_yScale);            
        } else {
            pathSample(sample, m_xScale * prev.x, m_yScale * prev.y, m_xScale * x, m_yScale * y, p.intensity);
        }
        prev.x = x;
        prev.y = y;
        m_samples.emplace_back(sample);
    }
    return stepCount;
}

struct IntegratorDevice::WinUSBDevice {
    WINUSB_INTERFACE_HANDLE husb = NULL;
    WINUSB_PIPE_INFORMATION inPipe;
    WINUSB_PIPE_INFORMATION outPipe;
};

bool IntegratorDevice::Connect()
{
    clearError();

    // Look up WinUSB device by the interface GUID
    GUID devguid = GUID_DEVINTERFACE_FAKETREX;
    auto link = findDeviceLink(devguid);
    if (link.empty()) {
        return false;
    }

    // Get handle, lookup and configure endpoints
    m_hdev = CreateFile(link.c_str(), GENERIC_WRITE | GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);

    if (m_hdev == INVALID_HANDLE_VALUE) {
        updateError("CreateFile", GetLastError());
        return false;
    }
    auto device = std::make_shared<WinUSBDevice>();

    if (!WinUsb_Initialize(m_hdev, &device->husb)) {
        updateError("WinUsb_Initialize", GetLastError());
        return false;
    }

    if (!WinUsb_QueryPipe(device->husb, 0, 0, &device->outPipe)) {
        updateError("WinUsb_QueryPipe", GetLastError());
        return false;
    }
    DWORD timeout = 3000;  // ms
    if (!WinUsb_SetPipePolicy(device->husb, device->outPipe.PipeId, PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout)) {
        updateError("WinUsb_SetPipePolicy", GetLastError());
        return false;
    }
    if (!WinUsb_QueryPipe(device->husb, 0, 1, &device->inPipe)) {
        updateError("WinUsb_QueryPipe", GetLastError());
        return false;
    }
    timeout = 3000;  // ms
    if (!WinUsb_SetPipePolicy(device->husb, device->inPipe.PipeId, PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout)) {
        updateError("WinUsb_SetPipePolicy", GetLastError());
        return false;
    }
    m_winusb = device;
    return true;
}

bool IntegratorDevice::WaitSync()
{
    if (m_winusb) {
        FTPacket inpacket;
        ULONG len = 0;
        if (!WinUsb_ReadPipe(m_winusb->husb, m_winusb->inPipe.PipeId, (PUCHAR)&inpacket, FT_MIN_PACKET_SIZE, &len, NULL)) {
            updateError("WinUsb_ReadPipe", GetLastError());
            return false;
        }
        return true;
    } else {
        updateError("WaitSync", ERROR_INVALID_STATE);
    }
    return false;
}

void IntegratorDevice::SetFrameDuration(int ms)
{
    ms = (int)(std::ceil(ms / 5.0f) * 5);
    m_frameDurationMs = std::clamp(ms, 5, 80);
}

void IntegratorDevice::Submit()
{
    if (!m_winusb) {
        updateError("Submit", ERROR_INVALID_STATE);
        return;
    }

    // build samples
    EncodeSamples(m_operations);

    // submit data
    if (m_samples.size() == 0) return;

    std::vector<byte> buffer(FT_MAX_PACKET_SIZE);
    FTPacket* packet = (FTPacket*)buffer.data();
    int packetc = 0;
    bool done = false;
    size_t si = 0;

    // Compose samples in packets and send them to the device
    do {
        memset(packet, 0, sizeof(FTPacket));
        packet->type = FT_P_TYPE_FRAME;
        packetc++;

        // fill a packet with samples
        int samplec = 0;
        for (; samplec < FT_MAX_SAMPLES && si < m_samples.size(); si++, samplec++) {
            packet->frame.samples[samplec] = m_samples[si];
        }
        setFramePacketSize(packet, samplec);

        if (packetc == 1) {
            packet->frame.sof = 1;  // first packet, start of frame
        }
        if (si == m_samples.size() || packetc == MAX_PACKETS_PER_FRAME) {
            packet->frame.eof = 1;  // last packet, end of frame
            done = true;
        }
        packet->frame.fps = m_frameDurationMs / 5;

        // send packet
        ULONG len;
        if (!WinUsb_WritePipe(m_winusb->husb, m_winusb->outPipe.PipeId, (PUCHAR)packet, packet->size, &len, NULL)) {
            updateError("WinUsb_WritePipe", GetLastError());
        }

    } while (!done);
}

IntegratorDevice::~IntegratorDevice() { Disconnect(); }

void IntegratorDevice::Disconnect()
{
    if (m_winusb) {
        if (m_winusb->husb) WinUsb_Free(m_winusb->husb);
        m_winusb = nullptr;
    }
    if (m_hdev != INVALID_HANDLE_VALUE) CloseHandle(m_hdev);
    m_hdev = INVALID_HANDLE_VALUE;
}

void IntegratorDevice::clearError()
{
    m_lastError = 0;
    m_lastErrorStr = "No error";
}

void IntegratorDevice::updateError(const char* msg, DWORD er)
{
    // Retrieve the system error message for the last-error code

    CHAR lpMsgBuf[250];
    CHAR lpDisplayBuf[250 + 80];

    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, er, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), lpMsgBuf,
        sizeof(lpMsgBuf), nullptr);

    StringCchPrintfA(lpDisplayBuf, sizeof(lpDisplayBuf), "%s Error %d: %s", msg, er, lpMsgBuf);

    // printf("%s\n", (char*)lpDisplayBuf);

    m_lastError = er;
    m_lastErrorStr = (char*)lpDisplayBuf;
}

/*
void readDeviceRegistry(HDEVINFO hDevInfo, PSP_DEVICE_INTERFACE_DATA pInterfaceData)
{
    // dev/TestKey
    //    TestValueKey DWORD
    HKEY hKey;
    HKEY hTestKey = (HKEY)(INVALID_HANDLE_VALUE);
    hKey = SetupDiOpenDeviceInterfaceRegKey(hDevInfo, pInterfaceData, 0, KEY_READ);
    if (hKey == INVALID_HANDLE_VALUE) {
        printError("SetupDiOpenDeviceInterfaceRegKey", GetLastError());
        goto _exit;
    }

    if (RegOpenKey(hKey, L"TestKey", &hTestKey) != ERROR_SUCCESS) {
        printError("RegOpenKey", GetLastError());
        goto _exit;
    }
    DWORD value = 0;
    DWORD len = sizeof(value);
    if (RegGetValue(hTestKey, NULL, L"TestValueKey", RRF_RT_REG_DWORD, NULL, &value, &len) != ERROR_SUCCESS) {
        printError("RegOpenKey", GetLastError());
        goto _exit;
    }
    // printf("Registry TestKey\\TestValueKey = %d\n", value);

_exit:
    if (hTestKey != INVALID_HANDLE_VALUE) RegCloseKey(hTestKey);

    if (hKey != INVALID_HANDLE_VALUE) RegCloseKey(hKey);
}
*/

std::wstring IntegratorDevice::findDeviceLink(const GUID& guid)
{
    std::wstring link;

    HDEVINFO hDevInfoSet = SetupDiGetClassDevs(&guid, nullptr, nullptr, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
    if (hDevInfoSet == INVALID_HANDLE_VALUE) return link;

    DWORD devIndex = 0;

    SP_DEVINFO_DATA devInfo;
    devInfo.cbSize = sizeof(SP_DEVINFO_DATA);
    SP_DEVICE_INTERFACE_DATA devInterfaceData;
    devInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    while (SetupDiEnumDeviceInterfaces(hDevInfoSet, nullptr, &guid, devIndex++, &devInterfaceData)) {
        DWORD reqSize{0};

        devInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
        SetupDiGetDeviceInterfaceDetailW(hDevInfoSet, &devInterfaceData, nullptr, 0, &reqSize, nullptr);

        SP_DEVICE_INTERFACE_DETAIL_DATA* devInterfaceDetailData = (SP_DEVICE_INTERFACE_DETAIL_DATA*)malloc(reqSize);

        devInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(hDevInfoSet, &devInterfaceData, devInterfaceDetailData, reqSize, nullptr, nullptr)) {
            updateError("SetupDiGetDeviceInterfaceDetail", GetLastError());
            free(devInterfaceDetailData);
            break;
        }

        // printf("Device found: %S\n", devInterfaceDetailData->DevicePath);
        link = devInterfaceDetailData->DevicePath;
        free(devInterfaceDetailData);
        // readDeviceRegistry(hDevInfoSet, &devInterfaceData);
    }

    SetupDiDestroyDeviceInfoList(hDevInfoSet);
    return link;
}

}  // namespace AudioRender