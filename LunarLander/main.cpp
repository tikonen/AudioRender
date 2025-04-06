#include <Windows.h>

#include <atomic>

#include <Log.hpp>

#include <AudioGraphics.hpp>
#include <AudioDevice.hpp>
#include <IntegratorDevice.hpp>

#include "cxxopts.hpp"
#include "game/Game.hpp"

#include <ToneSampleGenerator.hpp>
#include <SimulatorView.hpp>

BOOL WINAPI ctrlHandler(DWORD);
std::atomic_bool g_running = true;

#define VERSION "0.1"
#define APP_NAME "LunarLander"

#define X_SCALE (-2.f)
#define Y_SCALE (-2.f)

void mainLoop(AudioRender::IDrawDevice* device);

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
        ("flipx", "Flip x axis")         //
        ("flipy", "Flip y axis");

    try {
        result = options.parse(argc, argv);
    } catch (cxxopts::exceptions::exception ex) {
        printf("%s", ex.what());
        return 1;
    }

    const auto& unmatched = result.unmatched();
    if (result.count("help") || unmatched.size()) {
        printf("%s\n", options.help().c_str());
        for (auto& arg : unmatched) {
            printf("Unknown argument: \"%s\"\n", arg.c_str());
            break;
        }
        return 1;
    }

    float xScale = X_SCALE;
    float yScale = Y_SCALE;

    if (result.count("flipx")) xScale *= -1;
    if (result.count("flipy")) yScale *= -1;


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
        mainLoop(device);
    } else if (result.count("T")) {
        AudioRender::AudioDevice audioDevice;
        audioDevice.Initialize();
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

    } else if (result.count("A")) {
        AudioRender::AudioDevice audioDevice;
        audioDevice.Initialize();
        auto audioGenerator = std::make_shared<AudioRender::AudioGraphicsBuilder>();
        audioGenerator->setScale(xScale, yScale);
        audioDevice.SetGenerator(audioGenerator);
        audioDevice.Start();

        SetConsoleCtrlHandler(ctrlHandler, TRUE);

        AudioRender::IDrawDevice* drawDevice = audioGenerator.get();
        mainLoop(drawDevice);

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
            return 1;
        }

        // intDevice->setScale(X_SCALE, Y_SCALE);

        SetConsoleCtrlHandler(ctrlHandler, TRUE);

        AudioRender::IDrawDevice* drawDevice = intDevice.get();
        mainLoop(drawDevice);

        LOG("Stopping");
        intDevice->Disconnect();

    } else {
        printf("%s\n", options.help().c_str());
    }

    // Clear console input buffer before exit so keypresses won't flood on console command line on exit
    FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));

    return 0;
}

void mainLoop(AudioRender::IDrawDevice* device)
{
    LunarLander::Game game;
    game.mainLoop(g_running, device);
}

BOOL WINAPI ctrlHandler(DWORD dwCtrlType)
{
    g_running = false;
    return TRUE;
}
