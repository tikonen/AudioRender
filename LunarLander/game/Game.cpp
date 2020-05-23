#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <vector>

#include "Game.hpp"
#include "Terrain.hpp"

namespace LunarLander
{
void Game::mainLoop(std::atomic_bool& running, AudioRender::IDrawDevice* device)
{
    // Basic rendering loop
    float i = 0;
    const float pi = 3.414f;

    auto terrain = generateTerrain(0, 400);

    struct Point {
        int x, y;
    };

    struct ViewPort {
        int width;
        int height;
        Point pos;
        Point terrainPos;
        float zoom;
    } viewport;
    viewport.height = 200;
    viewport.width = 400;
    viewport.pos = {200, 100};
    viewport.terrainPos = {200, 100};
    viewport.zoom = 1;

    struct Button {
        virtual ~Button() = default;
        virtual bool status() = 0;
        virtual bool pressed() = 0;
    };

    struct Key : Button {
        bool keyDown = false;
        bool keyStatus = false;
        int keyCode;
        bool keyPressed = false;
        Key(int key)
            : keyCode(key)
        {
            keyDown = 0x8000 & GetKeyState(keyCode);
        }

        void update()
        {
            keyPressed = !keyDown && (0x8000 & GetKeyState(keyCode));
            keyDown = (0x8000 & GetKeyState(keyCode));
        }

        bool status() override { return keyDown; }
        bool pressed() override { return keyPressed; }
    };

    struct Controller {
        Key left = Key(VK_LEFT);
        Key right = Key(VK_RIGHT);
        Key up = Key(VK_UP);
        Key down = Key(VK_DOWN);
        Key zoomIn = Key(0x5A);   // Z
        Key zoomOut = Key(0x58);  // X

        void update()
        {
            left.update();
            right.update();
            up.update();
            down.update();
            zoomIn.update();
            zoomOut.update();
        }

    } controller;

    while (running) {
        device->Begin();
        device->SetIntensity(0.5f);

        controller.update();

        if (controller.zoomIn.status()) {
            viewport.zoom = std::min(viewport.zoom + 0.1f, 16.f);
        }
        if (controller.zoomOut.status()) {
            viewport.zoom = std::max(viewport.zoom - 0.1f, 1.f);
        }
        if (controller.left.status()) {
            viewport.pos.x -= 1;
        }
        if (controller.right.status()) {
            viewport.pos.x += 1;
        }
        if (controller.up.status()) {
            viewport.pos.y += 1;
        }
        if (controller.down.status()) {
            viewport.pos.y -= 1;
        }

        int windowWidth = std::lroundf(viewport.width / viewport.zoom);
        int windowHeight = std::lroundf(viewport.height / viewport.zoom);

        int terrainxs = terrain.size() / 2 - (viewport.terrainPos.x - viewport.pos.x) - windowWidth / 2;
        if (terrainxs < 0) terrainxs = 0;

        const int scale = 32;
        const int step = windowWidth / scale;
        int rounded = (terrainxs / step) * step;
        float delta = (terrainxs - rounded) / (float)windowWidth;
        terrainxs = rounded;

        int terrainxe = terrainxs + windowWidth;
        if (terrainxe >= terrain.size()) terrainxe = terrain.size();

        int terrainxy = std::lroundf((viewport.terrainPos.y - viewport.pos.y) / viewport.zoom);

        device->SetPoint({-0.5f - delta, (terrain[terrainxs] + terrainxy) / (float)viewport.height - 0.5f});
        terrainxs += step;
        for (size_t i = 1; i < windowWidth && terrainxs < terrainxe; i += step, terrainxs += step) {
            const int y = terrain[terrainxs] + terrainxy;
            device->DrawLine({i / (float)windowWidth - 0.5f - delta, y / (float)viewport.height - 0.5f});
        }
        device->WaitSync();
        device->Submit();
    }
    /*
    while (running) {
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
    */
}
}  // namespace LunarLander
