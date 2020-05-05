#include <Windows.h>

#include <condition_variable>
#include <mutex>
#include <memory>
#include <atomic>

#include <Log.hpp>

#include <AudioGraphics.hpp>
#include <AudioDevice.hpp>
#include <RasterImage.hpp>
#include <SVGImage.hpp>

#include "ToneSampleGenerator.hpp"
#include "View.hpp"

void DisplayUsage();
BOOL WINAPI ctrlHandler(DWORD);
std::atomic_bool g_running = true;

#define VERSION "0.1"
#define APP_NAME "AudioRenderAPITest"


void mainLoop(int demoMode, AudioRender::IDrawDevice* device);

int main(int argc, char* argv[])
{
    LOG("%s %s %s", APP_NAME, VERSION, __DATE__);

    bool audio = false;
    bool simulation = false;
    bool testTone = false;
    int demoMode = 1;

    // skip exe name
    argc--;
    argv++;

    while (argc-- > 0) {
        const char* arg = *argv++;
        if (!_stricmp(arg, "/h") || !_stricmp(arg, "/?")) {
            DisplayUsage();
        } else if (!_stricmp(arg, "/S")) {
            simulation = true;
        } else if (!_stricmp(arg, "/A")) {
            audio = true;
        } else if (!_stricmp(arg, "/T")) {
            testTone = true;
        } else if (!_stricmp(arg, "/D")) {
            if (argc >= 1) {
                demoMode = atoi(*argv);
                argv++;
                argc--;
            } else
                DisplayUsage();
        } else {
            DisplayUsage();
        }
    }

    if (simulation) {
        // Use simple window rendering
        std::shared_ptr<RenderView> renderView;
        renderView = std::make_shared<RenderView>(g_running);
        const char* background = "scope.jpg";
        if (!renderView->loadBackground(background)) {
            printf("WARNING: Cannot load %s\n.", background);
        }
        renderView->start();
        renderView->SimulateFlicker(true);
        renderView->SimulateBeamIdle(true);
        AudioRender::IDrawDevice* device = renderView.get();
        mainLoop(demoMode, device);
    } else if (audio) {
        AudioDevice audioDevice;
        audioDevice.Initialize();

        if (testTone) {
            const int frequency = 1000;
            auto toneGenerator = std::make_shared<ToneSampleGenerator>(frequency);
            audioDevice.SetGenerator(toneGenerator);
            LOG("Playing %0.1fkHz test tone", frequency / 1000.f);
            audioDevice.Start();

            SetConsoleCtrlHandler(ctrlHandler, TRUE);

            while (g_running) {
                Sleep(1000);
            }

            LOG("Stopping");
            audioDevice.Stop();
        } else {
            auto audioGenerator = std::make_shared<AudioRender::AudioGraphicsBuilder>();
            // Give some margin and flip Y axis
            audioGenerator->setScale(0.95f, -0.95f);
            audioDevice.SetGenerator(audioGenerator);
            audioDevice.Start();

            SetConsoleCtrlHandler(ctrlHandler, TRUE);

            AudioRender::IDrawDevice* drawDevice = audioGenerator.get();
            mainLoop(demoMode, drawDevice);

            LOG("Stopping");
            audioDevice.Stop();
        }
    } else {
        DisplayUsage();
    }
    return 0;
}

void basicRender(AudioRender::IDrawDevice* device)
{
    // Basic rendering loop
    float i = 0;
    const float pi = 3.414f;

    while (g_running) {
        device->Begin();
        device->SetIntensity(0.5f);
        device->SetPoint({0.0, 0.0});
        device->DrawCircle(0.5);

        const float rad = i++ / 360.f * pi;
        const float sinr = sin(rad);
        const float cosr = cos(rad);
        auto point = [=](float x, float y) { return AudioRender::Point{cosr * x - sinr * y, sinr * x + cosr * y}; };
        device->SetIntensity(0.3f);
        device->SetPoint(point(.0, .5f));
        device->DrawLine(point(.5f, .0));
        device->DrawLine(point(.0, -.5f));
        device->DrawLine(point(-.5f, .0f));
        device->DrawLine(point(.0, .5f));

        device->WaitSync();
        device->Submit();
    }
}

void rasterRender(AudioRender::IDrawDevice* device)
{
    // Raster graphics rendering

    // Load rasterizer
    std::shared_ptr<AudioRender::RasterImage> rasterizer;
    const char* filename = "nuclear.png";

    rasterizer = std::make_shared<AudioRender::RasterImage>();
    if (!rasterizer->loadImage(filename)) {
        rasterizer = nullptr;
        LOGE("Failed to load \"%s\". %s", filename, GetLastErrorString());
    }

    device->Begin();
    device->SetIntensity(0.2f);
    if (rasterizer) {
        rasterizer->drawImage(device);
    }
    // Keep showing the image
    while (g_running) {
        device->WaitSync();
        device->Submit();
    }
}

void svgRender(AudioRender::IDrawDevice* device)
{
    // SVG rendering

    // Load SVG
    std::shared_ptr<AudioRender::SVGImage> vectorizer;
    const char* SVGsamples[] = {
        "anarchy.svg", "communism.svg", "Heart-hand-shake.svg", "nano.svg", "nuclear.svg", "pentagram.svg", "SCP_Foundation.svg", "vault-tec-logo.svg"};

    unsigned int ts = 0;
    int imgidx = 0;
    bool spaceDown = false;

    while (g_running) {
        device->WaitSync();

        // Jump to next on space bar press
        bool spacePressed = !spaceDown && (0x8000 & GetKeyState(VK_SPACE));
        spaceDown = (0x8000 & GetKeyState(VK_SPACE));

        unsigned int now = GetTickCount();
        if (ts <= now || spacePressed) {
            // Swap image every 5 seconds
            ts = now + 5000;
            vectorizer = std::make_shared<AudioRender::SVGImage>();
            if (!vectorizer->loadImage(SVGsamples[imgidx])) {
                vectorizer = nullptr;
                LOGE("Failed to load \"%s\". %s", SVGsamples[imgidx], GetLastErrorString());
            }
            imgidx = (imgidx + 1) % ARRAYSIZE(SVGsamples);

            device->Begin();
            device->SetIntensity(0.5f);
            if (vectorizer) {
                vectorizer->drawImage(device, 1.0f);
            }
        }

        device->Submit();
    }
}

void mainLoop(int demoMode, AudioRender::IDrawDevice* device)
{
    if (demoMode == 1) {
        basicRender(device);
    } else if (demoMode == 2) {
        rasterRender(device);
    } else if (demoMode == 3) {
        svgRender(device);
    } else {
        LOGE("Invalid demomode %d\n", demoMode);
    }
}

BOOL WINAPI ctrlHandler(DWORD dwCtrlType)
{
    g_running = false;
    return TRUE;
}

void DisplayUsage()
{
    const char* msg = APP_NAME
        "\n"
        "Version: " VERSION
        "\n\n"
        "Usage:\n"
        "AudioRenderAPITest [options]\n"
        "\nOptions:\n"
        "/S\tVisual simulation.\n"
        "/A\tAudio render.\n"
        "/T\tTest tone.\n"
        "/D <mode>\tSpecify demo mode: 1 - Basic, 2 - Raster Image or 3 - SVG Graphics.\n"
        "/?\tPrint this message.\n"
        "\n";
    printf(msg);
    exit(1);
}
