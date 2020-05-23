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
        device->SetIntensity(0.2f);

        controller.update();

        if (controller.zoomIn.pressed()) {
            viewport.zoom = std::min(viewport.zoom + 1.f, 7.f);
        }
        if (controller.zoomOut.pressed()) {
            viewport.zoom = std::max(viewport.zoom - 1.f, 1.f);
        }
        if (controller.left.status()) {
            viewport.pos.x = std::max(viewport.pos.x - 1, 0);
        }
        if (controller.right.status()) {
            viewport.pos.x = std::min(viewport.pos.x + 1, viewport.width);
        }
        if (controller.down.status()) {
            viewport.pos.y = std::min(viewport.pos.y + 1, viewport.height);
        }
        if (controller.up.status()) {
            viewport.pos.y = std::max(viewport.pos.y - 1, 0);
        }

        int windowWidth = std::lroundf(viewport.width / viewport.zoom);
        int windowHeight = std::lroundf(viewport.height / viewport.zoom);

        int terrainxs = (int)terrain.size() / 2 - (viewport.terrainPos.x - viewport.pos.x) - windowWidth / 2;

        // Step terrain drawing on discrete intervals
        const int scale = 24;
        const int step = std::max(windowWidth / scale, 1);
        int rounded = (terrainxs / step) * step;
        float delta = (terrainxs - rounded) / (float)windowWidth;  // offset for smooth transition
        terrainxs = rounded;
        // sanity limits
        int terrainxe = terrainxs + windowWidth;
        if (terrainxe >= terrain.size()) terrainxe = (int)terrain.size();

        if (terrainxs < 0) {
            delta += terrainxs / (float)windowWidth;
            terrainxs = 0;
            terrainxe = terrainxs + windowWidth * (1 + delta);
        }

        int terrainyOffset = viewport.height / 2 - (viewport.terrainPos.y - viewport.pos.y) - windowHeight / 2;

        device->SetPoint({-0.5f - delta, (terrain[terrainxs] - terrainyOffset) / (float)windowHeight - 0.5f});
        terrainxs += step;
        for (size_t i = 1; i < windowWidth && terrainxs < terrainxe; i += step, terrainxs += step) {
            const int y = terrain[terrainxs] - terrainyOffset;
            device->DrawLine({i / (float)windowWidth - 0.5f - delta, y / (float)windowHeight - 0.5f});
        }

        // draw lander
        float landerScale = viewport.zoom * 0.03f;
        float height = 1.f;
        float width = 0.8f;
        device->SetIntensity(0.4f);
        device->SetPoint({0, -height / 2 * landerScale});
        device->DrawLine({-width / 2 * landerScale, 0});
        device->DrawLine({width / 2 * landerScale, 0});
        device->DrawLine({0, -height / 2 * landerScale});

        if (controller.up.status()) {
            device->SetIntensity(0.6f);
            device->SetPoint({0, height / 2 * landerScale});
            device->DrawLine({-width / 4 * landerScale, 0});
            device->SetPoint({0, height / 2 * landerScale});
            device->DrawLine({width / 4 * landerScale, 0});
        }

        device->WaitSync();
        device->Submit();
    }
}
}  // namespace LunarLander
