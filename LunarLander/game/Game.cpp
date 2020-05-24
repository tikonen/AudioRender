#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <vector>

#include <glm/glm.hpp>

#include "Game.hpp"
#include "Terrain.hpp"

#define G 1.625f  // ms^2

#define OUT_OF_BOUNDS 0

namespace LunarLander
{
void Game::mainLoop(std::atomic_bool& running, AudioRender::IDrawDevice* device)
{
    float i = 0;

    auto terrain = generateTerrain(0, 400);

    using Vector2D = glm::ivec2;
    using Vector2Df = glm::vec2;

    struct ViewPort {
        int width;
        int height;
        Vector2D pos;
        Vector2D terrainPos;
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

    struct Lander {
        Vector2Df A = {0, G / 3.f};
        Vector2Df velocity{0};
        Vector2D pos{0};
        float angle = 0;  // in radians
        const float height = 1.f;
        const float width = 0.5f;
        void update(float t)
        {
            // TODO position
        }

    } lander;

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

        const int windowWidth = std::lroundf(viewport.width / viewport.zoom);
        const int windowHeight = std::lroundf(viewport.height / viewport.zoom);

        int terrainxs = (int)terrain.size() / 2 - (viewport.terrainPos.x - viewport.pos.x) - windowWidth / 2;

#if !OUT_OF_BOUNDS
        if (terrainxs < 0) {
            viewport.pos.x = windowWidth / 2;
            terrainxs = 0;
        }
        if (terrainxs + windowWidth > terrain.size()) {
            terrainxs = (int)terrain.size() - windowWidth;
            viewport.pos.x = viewport.width - windowWidth / 2;
        }
#endif

        // Step terrain drawing on discrete intervals
        const int scale = 24;
        const int step = std::max(windowWidth / scale, 1);
        int rounded = (terrainxs / step) * step;
        float delta = (terrainxs - rounded) / (float)windowWidth;  // offset for smooth transition
        terrainxs = rounded;

        int terrainxe = terrainxs + windowWidth;
        // sanity limits
        if (terrainxe > terrain.size()) {
            terrainxe = (int)terrain.size();
        }
#if OUT_OF_BOUNDS
        // allow out of bounds
        if (terrainxe >= terrain.size()) terrainxe = (int)terrain.size();

        if (terrainxs < 0) {
            delta += terrainxs / (float)windowWidth;
            terrainxs = 0;
            terrainxe = terrainxs + (int)(windowWidth * (1 + delta));
        }
#endif

        int terrainyOffset = viewport.height / 2 - (viewport.terrainPos.y - viewport.pos.y) - windowHeight / 2;

        // Draw terrain
        device->SetPoint({-0.5f - delta, (terrain[terrainxs] - terrainyOffset) / (float)windowHeight - 0.5f});
        terrainxs += step;
        for (size_t i = 1; i < windowWidth && terrainxs < terrainxe; i += step, terrainxs += step) {
            const int y = terrain[terrainxs] - terrainyOffset;
            device->DrawLine({i / (float)windowWidth - 0.5f - delta, y / (float)windowHeight - 0.5f});
        }

        // draw lander
        float landerScale = viewport.zoom * 0.03f;
        device->SetIntensity(0.4f);
        device->SetPoint({0, -lander.height / 3 * landerScale});
        device->DrawLine({-lander.width / 2 * landerScale, 0});
        device->DrawLine({lander.width / 2 * landerScale, 0});
        device->DrawLine({0, -lander.height / 3 * landerScale});

        if (controller.up.status()) {
            // flame.
            // ##TODO randomize flame size
            device->SetIntensity(0.6f);
            device->SetPoint({0, lander.height / 2 * landerScale});
            device->DrawLine({-lander.width / 4 * landerScale, 0});
            device->SetPoint({0, lander.height / 2 * landerScale});
            device->DrawLine({lander.width / 4 * landerScale, 0});
        }

        device->WaitSync();
        device->Submit();
    }
}
}  // namespace LunarLander
