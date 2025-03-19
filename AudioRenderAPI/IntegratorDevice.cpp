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
#define SPEED_SCALE 4500
#define INTEG_SCALE_FACTOR 0.25f

IntegratorGraphicsBuilder::IntegratorGraphicsBuilder()
    : m_xScale(INTEG_SCALE_FACTOR)
    , m_yScale(INTEG_SCALE_FACTOR)
{
}

void IntegratorGraphicsBuilder::setScale(float xscale, float yscale)
{
    m_xScale = xscale * INTEG_SCALE_FACTOR;
    m_yScale = yscale * INTEG_SCALE_FACTOR;
}

static float norm(float x0, float y0, float x1, float y1)
{
    const float xd = (x1 - x0);
    const float yd = (y1 - y0);
    return sqrtf(xd * xd + yd * yd);
}

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

static void controlSample(FTSample& sample, bool reset, bool nodac, int wait)
{
    sample.resetx = reset;
    sample.x = 0;
    sample.resety = reset;
    sample.y = 0;
    sample.nodac = nodac;
    sample.wait = wait;
}

static void pointSample(FTSample& sample, float x0, float y0, bool reset)
{
    sample.resetx = reset;
    sample.x = dacMap(x0);
    sample.resety = reset;
    sample.y = dacMap(y0);
    sample.nodac = 0;
    sample.wait = INTEG_STABILIZATION_TIME_US;  // stabilization time
}

static void pathSample(FTSample& sample, float xref, float yref, float x0, float y0, float x1, float y1, float speed)
{
    const float t = SPEED_SCALE * norm(x0, y0, x1, y1) * 1e-6f * speed;
    const float Vdx = vDeltaEst(x1 - x0, t);
    const float Vdy = vDeltaEst(y1 - y0, t);

    sample.resetx = 0;
    sample.x = dacMap(xref + Vdx);
    sample.resety = 0;
    sample.y = dacMap(yref + Vdy);
    sample.nodac = 0;
    sample.wait = (uint16_t)(t * 1e6f);  // TODO check max value
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

int IntegratorGraphicsBuilder::encodeSync(float x, float y, EncodeCtx& ctx)
{
    int samplec = 0;
    FTSample sample;

    ctx.xref = x * m_xScale;
    ctx.yref = y * m_yScale;
    ctx.syncPoint = true;

    // Reset the integrator to a specified levels
    pointSample(sample, ctx.xref, ctx.yref, true);
    m_samples.emplace_back(sample);
    samplec++;

    // Lock integrator reference and wait a few us to stabilize
    controlSample(sample, false, true, 4);
    m_samples.emplace_back(sample);
    samplec++;

    return samplec;
}

int IntegratorGraphicsBuilder::EncodeSync(const GraphicsPrimitive& p, EncodeCtx& ctx)
{
    return encodeSync(p.p.x, p.p.y, ctx);
}

int IntegratorGraphicsBuilder::EncodeLine(const GraphicsPrimitive& p, EncodeCtx& ctx)
{
    FTSample sample;

    ctx.syncPoint = false;

    pathSample(sample, ctx.xref, ctx.yref, m_xScale * p.p.x, m_yScale * p.p.y, m_xScale * p.toPoint.x, m_yScale * p.toPoint.y, p.intensity);
    m_samples.emplace_back(sample);    

    return 1;
}

int IntegratorGraphicsBuilder::EncodeCircle(const GraphicsPrimitive& p, EncodeCtx& ctx)
{
    const float CircleSegmentMultiplier = 100.0f;  // how many segments in unit circle
    const float Pi = 3.14159f;
    const int stepCount = (int)std::ceil(CircleSegmentMultiplier * p.r * p.intensity);
    const float angleStep = Pi * 2 / stepCount;

    Point prev;
    for (int i = 0; i < stepCount + 1; i++) {
        float x = p.r * sinf(i * angleStep) + p.p.x;
        float y = p.r * cosf(i * angleStep) + p.p.y;

        if (i == 0) {
            encodeSync(x, y, ctx);
        } else {
            FTSample sample;
            pathSample(sample, ctx.xref, ctx.yref, m_xScale * prev.x, m_yScale * prev.y, m_xScale * x, m_yScale * y, p.intensity);
            m_samples.emplace_back(sample);
        }
        prev.x = x;
        prev.y = y;
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

bool IntegratorDevice::WaitSync(int timeoutms)
{
    (void)timeoutms;  // ignore the parameter and always use hardcoded internal timeout
    FTPacket inpacket;
    return receivePacket(&inpacket);
}

bool IntegratorDevice::receivePacket(FTPacket* inpacket)
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

        sendPacket(packet);

    } while (!done);
}

bool IntegratorDevice::sendPacket(const FTPacket* packet)
{
    if (!m_winusb) {
        updateError("Submit", ERROR_INVALID_STATE);
        return false;
    } else {
        // send packet
        ULONG len;
        if (!WinUsb_WritePipe(m_winusb->husb, m_winusb->outPipe.PipeId, (const PUCHAR)packet, packet->size, &len, NULL)) {
            updateError("WinUsb_WritePipe", GetLastError());
            return false;
        }
    }
    return true;
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