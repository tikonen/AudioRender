#include <Windows.h>

#include <atomic>

#include <Log.hpp>

#include <AudioGraphics.hpp>
#include <AudioDevice.hpp>
#include <IntegratorDevice.hpp>
#include <RasterImage.hpp>
#include <SVGImage.hpp>

#include "cxxopts.hpp"

#include "ToneSampleGenerator.hpp"
#include "SimulatorView.hpp"

BOOL WINAPI ctrlHandler(DWORD);
std::atomic_bool g_running = true;

#define VERSION "0.2"
#define APP_NAME "AudioRenderAPITest"

void mainLoop(int demoMode, AudioRender::IDrawDevice* device);

int main(int argc, char* argv[])
{
    cxxopts::Options options(argv[0], APP_NAME " " VERSION " " __DATE__);
    cxxopts::ParseResult result;

    options.add_options()                //
        ("h,help", "This message")       //
        ("S", "Simulation render")       //
        ("A", "Audio render")            //
        ("I", "Integrator render")       //
        ("T", "Test audio tone render")  //
        ("D", "Demo mode (1 Basic, 2: Raster Image or 3: SVG Graphics)", cxxopts::value<int>()->default_value("1"));

    try {
        result = options.parse(argc, argv);
    } catch (cxxopts::exceptions::exception ex) {
        printf("%s", ex.what());
        return 1;
    }
    if (result.count("help")) {
        printf("%s\n", options.help().c_str());
        return 1;
    }

    int demoMode = result["D"].as<int>();

    if (result.count("S")) {
        // Use simple window rendering
        std::shared_ptr<SimulatorRenderView> renderView;
        renderView = std::make_shared<SimulatorRenderView>(g_running);
        const char* background = "scope.jpg";
        if (!renderView->loadBackground(background)) {
            printf("WARNING: Cannot load %s\n.", background);
        }
        renderView->start();
        renderView->SimulateFlicker(true);
        renderView->SimulateBeamIdle(true);
        AudioRender::IDrawDevice* device = renderView.get();
        mainLoop(demoMode, device);
    } else if (result.count("A")) {
        AudioRender::AudioDevice audioDevice;
        AudioRender::AudioDevice::Configuration config;

        // Following configuration chooses the default audio device
        // config.deviceName = L"";
        // config.fallBackToCommunicationDevice = false;

        audioDevice.InitializeWithConfig(config);

        auto audioGenerator = std::make_shared<AudioRender::AudioGraphicsBuilder>();
        // Give some margin and flip Y axis. Very old tube scopes render sometimes y-axis upside down.
        // If image shows sideways on the scope, swap channel 1 and  channel 2 wires to the scope.
        audioGenerator->setScale(0.95f, -0.95f);
        if (demoMode == 1) {
            // Mode 1 has so little data that without this setting the render would spin too way fast
            audioGenerator->setFixedRenderingRate(true);
        } else if (demoMode == 3) {
            // flip x axis also for SVG images
            audioGenerator->setScale(-0.95f, -0.95f);
        }
        audioDevice.SetGenerator(audioGenerator);
        audioDevice.Start();

        SetConsoleCtrlHandler(ctrlHandler, TRUE);
        LOG("Ctrl-C to break.");

        AudioRender::IDrawDevice* drawDevice = audioGenerator.get();
        mainLoop(demoMode, drawDevice);

        LOG("Stopping");
        audioDevice.Stop();
    } else if (result.count("I")) {
        auto intDevice = std::make_shared<AudioRender::IntegratorDevice>();

        if (!intDevice->Connect()) {

            LOG("Cannot connect to integrator");
            DWORD err = intDevice->lastError();
            if (err) {
                LOG("Error: %d %s", err, intDevice->lastErrorStr());
            } else {
                LOG("Device not found.");
            }
#if 0
            return 1;
#endif
        } else {
            LOG("Connected to integrator");
        }

        SetConsoleCtrlHandler(ctrlHandler, TRUE);
        LOG("Ctrl-C to break.");

        AudioRender::IDrawDevice* drawDevice = intDevice.get();
        mainLoop(demoMode, drawDevice);

        LOG("Stopping");
        intDevice->Disconnect();

    } else if (result.count("T")) {
        AudioRender::AudioDevice audioDevice;
        AudioRender::AudioDevice::Configuration config;

        // Following configuration chooses the default audio device
        // config.deviceName = L"";
        // config.fallBackToCommunicationDevice = false;

        audioDevice.InitializeWithConfig(config);

        const int frequency = 1000;
        auto audioGenerator = std::make_shared<ToneSampleGenerator>(frequency);
        audioDevice.SetGenerator(audioGenerator);
        LOG("Playing %0.1fkHz test tone", frequency / 1000.f);
        audioDevice.Start();

        SetConsoleCtrlHandler(ctrlHandler, TRUE);
        LOG("Ctrl-C to break.");

        while (g_running) {
            Sleep(1000);
        }

        LOG("Stopping");
        audioDevice.Stop();
    } else {
        printf("%s\n", options.help().c_str());
        return 1;
    }
    // Clear console input buffer before exit so keypresses won't flood on console command line on exit
    FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));
    return 0;
}

#if 0
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

        if (!device->WaitSync(1000)) g_running = false;
        device->Submit();
    }
}
#else
void basicRender(AudioRender::IDrawDevice* device)
{
    // Basic rendering loop
    float i = 0;
    const float pi = 3.414f;
 
    while (g_running) {
        device->Begin();

        device->SetIntensity(0.3f);
        device->SetPoint({0.0, 0.0});
        device->DrawCircle(0.5);

        const float rad = i++ / 360.f * pi;        
        const float sinr = sin(rad);
        const float cosr = cos(rad);
        auto point = [=](float x, float y) { return AudioRender::Point{cosr * x - sinr * y, sinr * x + cosr * y}; };
        //device->SetIntensity(0.25f);
        device->SetIntensity(0.25f);
        device->SetPoint(point(.0f, .0f));
        //device->SetPoint(point(.0, .5f));
        device->DrawLine(point(.0f, .5f));
        device->DrawLine(point(.5f, .0));
        device->DrawLine(point(.0, -.5f));
        device->DrawLine(point(-.5f, .0f));
        device->DrawLine(point(.0, .5f));

        /*
        device->SetPoint({0.0f, 0.0f});    
        device->DrawLine({.5f, .5f});
        device->DrawLine({.5f, -.5f});
        device->DrawLine({0.0f, 0.0f});
        */

        /*
        auto point = [=](float x, float y) { return AudioRender::Point{x, y}; };
        device->SetPoint(point(.0f, .0f));
        device->DrawLine(point(.0, .5f));
        device->DrawLine(point(.5f, .0));
        device->DrawLine(point(.0, -.5f));
        device->DrawLine(point(-.5f, .0f));
        device->DrawLine(point(.0, .5f));
        */

        if (!device->WaitSync(1000)) g_running = false;
        device->Submit();
    }
}
#endif

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
        if (!device->WaitSync(1000)) g_running = false;
        device->Submit();
    }
}

void svgRender(AudioRender::IDrawDevice* device)
{
    // SVG rendering
    printf("========================================================================\n");
    printf("Key up and Key down - Experiment with the line intensity.\n");
    printf("Space Bar - Cycle images.\n");
    printf("Q - Quit\n");


    // Load SVG
    std::shared_ptr<AudioRender::SVGImage> vectorizer;
    const char* SVGsamples[] = {"anarchy.svg", "communism.svg", "Heart-hand-shake.svg", "poison.svg", "nano.svg", "nuclear.svg", "pentagram.svg",
        "SCP_Foundation.svg", "vault-tec-logo.svg"};

    unsigned int ts = 0;
    int imgidx = 0;
    bool spaceDown = false;
    float intensity = 0.5f;
    bool keyUpDown = false;
    bool keyDownDown = false;
    bool keyPDown = false;
    bool paused = false;

    while (g_running) {
        
        // Jump to next on space bar press
        bool spacePressed = !spaceDown && (0x8000 & GetKeyState(VK_SPACE));
        spaceDown = (0x8000 & GetKeyState(VK_SPACE));

        bool keyUpPressed = !keyUpDown && (0x8000 & GetKeyState(VK_UP));
        keyUpDown = (0x8000 & GetKeyState(VK_UP));

        bool keyDownPressed = !keyDownDown && (0x8000 & GetKeyState(VK_DOWN));
        keyDownDown = (0x8000 & GetKeyState(VK_DOWN));

        bool keyPPressed = !keyPDown && (0x8000 & GetKeyState('P'));
        keyPDown = (0x8000 & GetKeyState('P'));

        if (0x8000 & GetKeyState(0x51)) {  // 'Q'
            g_running = false;
            continue;
        }

        if (keyUpPressed) {
            intensity += 0.1f;
        }
        if (keyDownPressed) {
            intensity -= 0.1f;
            if (intensity <= 0.1f) intensity = 0.1f;
        }
        if (keyPPressed) {
            paused = !paused;
            if (paused) {
                printf("Paused\n");    
            } else {
                printf("Unpaused\n");
            }
        }

        if (keyUpPressed || keyDownPressed) printf("Intensity %.1f\n", intensity);

        unsigned int now = GetTickCount();
        bool imageSwapped = (ts <= now || spacePressed) && !paused;
        if (imageSwapped) {
            // Swap image every 8 seconds
            ts = now + 8000;
            vectorizer = std::make_shared<AudioRender::SVGImage>();
            if (!vectorizer->loadImage(SVGsamples[imgidx])) {
                vectorizer = nullptr;
                LOGE("Failed to load \"%s\". %s", SVGsamples[imgidx], GetLastErrorString());
            } else {
                printf("# %s\n", SVGsamples[imgidx]);
            }
            imgidx = (imgidx + 1) % ARRAYSIZE(SVGsamples);
            
        }
        if (imageSwapped || keyUpPressed || keyDownPressed) {
            device->Begin();
            device->SetIntensity(intensity);
            if (vectorizer) {
                vectorizer->drawImage(device, 1.8f);
            }
        }
        
        if (!device->WaitSync(1000)) g_running = false;
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
