#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <vector>
#include <chrono>

#define _USE_MATH_DEFINES
#include <math.h>

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>
#undef GLM_ENABLE_EXPERIMENTAL

#include "Game.hpp"
#include "Terrain.hpp"

#define G 1.625f  // ms^2

namespace LunarLander
{
void Game::mainLoop(std::atomic_bool& running, AudioRender::IDrawDevice* device)
{
    float i = 0;

    auto map = generateTerrain(0, 400);
    auto terrain = map.terrain;
    auto landingPlaces = map.landingPlaces;

    using Vector2D = glm::ivec2;
    using Vector2Df = glm::vec2;

    struct ViewPort {
        int width;
        int height;
        Vector2Df pos;
        Vector2D terrainPos;
        float zoom;
    } viewport;
    viewport.height = 200;
    viewport.width = 400;
    viewport.pos = {200.f, 100.f};
    viewport.terrainPos = {200, 100};
    viewport.zoom = 3;

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
        Vector2Df A = {0, G};
        Vector2Df velocity{0};
        Vector2Df pos{200, 100};
        float angularSpeed = 0;
        float angle = 0;  // in radians
        const float height = 1.f;
        const float width = 0.5f;
        const float thrust = 3 * G;
#define DEGTORAD(d) ((float)M_PI / 180.f * (d))
        const float angularAcc = DEGTORAD(20);  // d/s^2

        void update(float t, int engine, int rotation)
        {
            // lander rotation
            const float dampening = 0.90f;

            float adelta = angularAcc * rotation * t;
            angle += (angularSpeed + adelta / 2.f) * t;
            angularSpeed += adelta;
            if (!rotation) angularSpeed -= angularSpeed * dampening * t;

            // lander position
            const float accScale = 1 / 2.f;

            Vector2Df tvec = Vector2Df(0, engine * -thrust);
            Vector2Df totalAcc = A + glm::rotate(tvec, angle);

            Vector2Df vdelta = totalAcc * t * accScale;
            pos += (velocity + vdelta / 2.f) * t;
            velocity += vdelta;
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
            // viewport.pos.x = std::max(viewport.pos.x - 1, 0.f);
        }
        if (controller.right.status()) {
            // viewport.pos.x = std::min(viewport.pos.x + 1, (float)viewport.width);
        }
        if (controller.down.status()) {
            // viewport.pos.y = std::min(viewport.pos.y + 1, viewport.height);
        }
        if (controller.up.status()) {
            // viewport.pos.y = std::max(viewport.pos.y - 1, 0);
        }

        viewport.pos.x = lander.pos.x;
        viewport.pos.y = lander.pos.y;

        const int windowWidth = std::lroundf(viewport.width / viewport.zoom);
        const int windowHeight = std::lroundf(viewport.height / viewport.zoom);

        int terrainxs = (int)terrain.size() / 2 - (viewport.terrainPos.x - std::lroundf(viewport.pos.x)) - windowWidth / 2;

        if (terrainxs < 0) {
            viewport.pos.x = windowWidth / 2.f;
            terrainxs = 0;
        }
        if (terrainxs + windowWidth > terrain.size()) {
            terrainxs = (int)terrain.size() - windowWidth;
            viewport.pos.x = viewport.width - windowWidth / 2.f;
        }
        // TODO y coordinate checks

        // Step terrain drawing on discrete intervals
        const int scale = 32;
        const int step = std::max(windowWidth / scale, 1);
        int rounded = (terrainxs / step) * step;
        float delta = (terrainxs - rounded) / (float)windowWidth;  // offset for smooth transition
        delta += (viewport.pos.x - std::lroundf(viewport.pos.x)) / windowWidth;
        terrainxs = rounded;

        int terrainxe = terrainxs + windowWidth;
        // sanity limits
        if (terrainxe > terrain.size()) {
            terrainxe = (int)terrain.size();
        }

        float terrainyOffset = viewport.height / 2 - (viewport.terrainPos.y - viewport.pos.y) - windowHeight / 2;

        // Draw terrain
        int xs = terrainxs;
        device->SetPoint({-0.5f - delta, (terrain[xs] - terrainyOffset) / (float)windowHeight - 0.5f});
        xs += step;
        for (size_t i = 1; i < windowWidth && xs < terrainxe; i += step, xs += step) {
            const float y = terrain[xs] - terrainyOffset;
            device->DrawLine({i / (float)windowWidth - 0.5f - delta, y / (float)windowHeight - 0.5f});
        }

        // Mark landing places
        device->SetIntensity(0.8f);
        for (auto& p : landingPlaces) {
            if (p.first > terrainxe) continue;
            if (p.second < terrainxs) continue;

            const float y = terrain[p.first] - terrainyOffset + 0.1f;
            device->SetPoint({(p.first - terrainxs) / (float)windowWidth - 0.5f - delta, y / (float)windowHeight - 0.5f});
            device->DrawLine({(p.second - terrainxs) / (float)windowWidth - 0.5f - delta, y / (float)windowHeight - 0.5f});
        }

        // Update lander
        using namespace std::chrono;
        static auto ticks = system_clock::now();
        auto now = system_clock::now();
        long long elapsed = duration_cast<milliseconds>(now - ticks).count();
        ticks = now;
        float dt = elapsed / 1000.f;
        bool engineon = controller.up.status();
        int rotation = 0;
        if (controller.left.status()) {
            rotation -= 1;
        }
        if (controller.right.status()) {
            rotation += 1;
        }
        lander.update(dt, engineon, rotation);

        // draw lander
        float landerScale = viewport.zoom * 0.03f;
        device->SetIntensity(0.4f);

        // TODO collision

        // lander position
        Vector2Df offset = lander.pos - viewport.pos;
        offset.x /= windowWidth;
        offset.y /= windowHeight;

        auto rotatedPoint = [&](float x, float y) -> AudioRender::Point {
            auto v = glm::rotate(glm::vec2(x, y), lander.angle);
            v += offset;
            return {v.x, v.y};
        };

        // TODO rotation point in the middle of mass
        device->SetPoint(rotatedPoint(0, -lander.height / 3 * landerScale));
        device->DrawLine(rotatedPoint(-lander.width / 2 * landerScale, 0));
        device->DrawLine(rotatedPoint(lander.width / 2 * landerScale, 0));
        device->DrawLine(rotatedPoint(0, -lander.height / 3 * landerScale));

        if (engineon) {
            // draw engine exhaust
            device->SetIntensity(0.4f);

            // vary exhaust size
            float h = lander.height / 2 * landerScale;
            h += (elapsed & 0x3) * h / 6.f;

            device->SetPoint(rotatedPoint(0, h));
            device->DrawLine(rotatedPoint(-lander.width / 4 * landerScale, 0));
            device->SetPoint(rotatedPoint(0, h));
            device->DrawLine(rotatedPoint(lander.width / 4 * landerScale, 0));
        }

        // TODO ground radar should control zoom

        device->WaitSync();
        device->Submit();
    }
}
}  // namespace LunarLander
