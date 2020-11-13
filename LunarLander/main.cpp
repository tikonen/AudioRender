#include <Windows.h>

#include <atomic>

#include <Log.hpp>

#include <AudioGraphics.hpp>
#include <AudioDevice.hpp>

#include "game/Game.hpp"

#include "ToneSampleGenerator.hpp"
#include "View.hpp"


void DisplayUsage();
BOOL WINAPI ctrlHandler(DWORD);
std::atomic_bool g_running = true;

#define VERSION "0.1"
#define APP_NAME "LunarLander"

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
            /*
        } else if (!_stricmp(arg, "/D")) {
            if (argc >= 1) {
                demoMode = atoi(*argv);
                argv++;
                argc--;
            } else
                DisplayUsage();
                */
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
            // audioGenerator->setScale(-0.95f, -0.95f);
            audioGenerator->setScale(-2.f, -2.f);
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

void mainLoop(int demoMode, AudioRender::IDrawDevice* device)
{
    LunarLander::Game game;
    game.mainLoop(g_running, device);
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
        "Usage:\n"  //
        APP_NAME
        " [options]\n"
        "\nOptions:\n"
        "/S\tVisual simulation.\n"
        "/A\tAudio render.\n"
        "/T\tTest tone.\n"
        //"/D <mode>\tSpecify demo mode: 1 - Basic, 2 - Raster Image or 3 - SVG Graphics.\n"
        "/?\tPrint this message.\n"
        "\n";
    printf(msg);
    exit(1);
}
