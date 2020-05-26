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
    viewport.height = 400;
    viewport.width = 400;
    viewport.pos = {200.f, 150.f};
    viewport.terrainPos = {0, 0};
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
            int state = 0x8000 & GetKeyState(keyCode);
            keyPressed = !keyDown && state;
            keyDown = state;
        }

        bool status() override { return keyDown; }
        bool pressed() override { return keyPressed; }
    };

    struct Controller {
        Key left = Key(VK_LEFT);
        Key right = Key(VK_RIGHT);
        Key throttle = Key(VK_UP);
        Key down = Key(VK_DOWN);
        Key zoomIn = Key(0x5A);   // Z
        Key zoomOut = Key(0x58);  // X
        Key pause = Key(0x50);    // P

        void update()
        {
            left.update();
            right.update();
            throttle.update();
            down.update();
            zoomIn.update();
            zoomOut.update();
            pause.update();
        }

    } controller;

    struct Lander {
        Vector2Df A = {0, G};
        Vector2Df velocity{0};
        Vector2Df pos{0};
        float angularSpeed = 0;
        float angle = 0;  // in radians
        const float height = 5.f;
        const float width = 5.f;
        const float thrust = 3 * G;
#define DEGTORAD(d) ((float)M_PI / 180.f * (d))
        const float angularAcc = DEGTORAD(20);  // d/s^2

        void update(float t, int engine, int rotation)
        {
            // lander rotation
            const float dampening = 0.80f;

            float adelta = angularAcc * rotation * t;
            angle += (angularSpeed + adelta / 2.f) * t;
            angularSpeed += adelta;
            if (!rotation) angularSpeed -= angularSpeed * dampening * t;
            if (angle > (float)M_PI) angle = -(2 * (float)M_PI - angle);
            if (angle < (float)-M_PI) angle = (2 * (float)M_PI + angle);

            // lander position
            const float accScale = 1 / 2.f;

            Vector2Df tvec = Vector2Df(0, engine * -thrust);
            Vector2Df totalAcc = A + glm::rotate(tvec, angle);

            Vector2Df vdelta = totalAcc * t * accScale;
            pos += (velocity + vdelta / 2.f) * t;
            velocity += vdelta;
        }

    } lander;
    lander.pos = {viewport.pos.x, 50};

    enum GameState { ST_WAIT, ST_PLAY, ST_LANDED, ST_FAIL } gameState;
    gameState = ST_WAIT;

    bool paused = false;

    using Letter = std::vector<std::vector<AudioRender::Point>>;

    const Letter l_a = {{{0, 0}, {2.5f, -10}, {5.f, 0}}, {{2.5f / 2, -4}, {5.f - 2.5f / 2, -4}}};
    const Letter l_f = {{{0, 0}, {0, -10}, {4, -10}}, {{0, -5.f}, {3, -5.f}}};
    const Letter l_i = {{{0, 0}, {0, -10}}};
    const Letter l_k = {{{0, 0}, {0, -10}}, {{4, -10}, {0, -5}, {4, 0}}};
    const Letter l_l = {{{0, -10}, {0, 0}, {3.5f, 0}}};
    const Letter l_o = {{{3, 0}, {6, -5}, {3, -10}, {0, -5}, {3, 0}}};
    const Letter l_p = {{{0, 0}, {0, -10}, {5, -7}, {0, -3}}};
    const Letter l_r = {{{0, 0}, {0, -10}, {5, -7}, {0, -3}, {5, 0}}};
    const Letter l_w = {{{0, -10}, {6.f / 3, 0}, {6.f / 2, -6}, {6.f - 6.f / 3, 0}, {6, -10}}};
    const Letter l_n = {{{0, 0}, {0, -10}, {4, 0}, {4, -10}}};

    float letterScale = 1.f / 10 * 0.2f;
    auto drawLetter = [&](const Letter& lf, AudioRender::Point offset) {
        for (const auto& seg : lf) {
            device->SetPoint((seg[0] + offset) * letterScale);
            for (size_t i = 1; i < seg.size(); i++) {
                device->DrawLine((seg[i] + offset) * letterScale);
            }
        }
    };

    while (running) {
        using namespace std::chrono;
        static auto ticks = system_clock::now();
        auto now = system_clock::now();
        long long elapsed = duration_cast<milliseconds>(now - ticks).count();

        if (elapsed < 1000 / 100) {  // Sanity limit
            Sleep(1);
            continue;
        }
        ticks = now;

        controller.update();

        if (controller.pause.pressed()) {
            paused = !paused;
        }

        if (paused) {
            // just show last render
            device->WaitSync();
            device->Submit();
            continue;
        }
        // clear frame
        device->Begin();
        device->SetIntensity(0.2f);

        if (controller.zoomIn.pressed()) {
            viewport.zoom = std::min(viewport.zoom + 1.f, 15.f);
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
        if (controller.throttle.status()) {
            // viewport.pos.y = std::max(viewport.pos.y - 1, 0);
        }

        viewport.pos.x = lander.pos.x;
        viewport.pos.y = lander.pos.y;

        const int windowWidth = std::lroundf(viewport.width / viewport.zoom);
        const float windowScale = 1.f / windowWidth;

        int terrainxs = viewport.terrainPos.x + std::lroundf(viewport.pos.x) - windowWidth / 2;

        if (terrainxs < 0) {
            viewport.pos.x = windowWidth / 2.f;
            terrainxs = 0;
        }
        if (terrainxs + windowWidth > terrain.size()) {
            terrainxs = (int)terrain.size() - windowWidth;
            viewport.pos.x = viewport.width - windowWidth / 2.f;
        }

        // Step terrain drawing on discrete intervals
        const int scale = 32;
        const int step = std::max(windowWidth / scale, 1);
        int rounded = (terrainxs / step) * step;
        terrainxs = rounded;

        int terrainxe = terrainxs + windowWidth;
        // sanity limits
        if (terrainxe > terrain.size()) {
            terrainxe = (int)terrain.size();
        }

        float terrainyOffset = viewport.pos.y - viewport.terrainPos.y;

        // Draw terrain
        int xs = terrainxs;

        device->SetPoint({(xs - viewport.pos.x) * windowScale, (terrain[xs] - terrainyOffset) * windowScale});
        xs += step;
        for (; xs < terrainxe; xs += step) {
            const float x = (xs - viewport.pos.x) * windowScale;
            const float y = (terrain[xs] - terrainyOffset) * windowScale;
            device->DrawLine({x, y});
        }

        // Mark landing places
        device->SetIntensity(0.6f);
        for (auto& p : landingPlaces) {
            if (p.first > terrainxe) continue;
            if (p.second < terrainxs) continue;

            const float y = (terrain[p.first] - terrainyOffset + 1) * windowScale;
            device->SetPoint({(p.first - viewport.pos.x) * windowScale, y});
            device->DrawLine({(p.second - viewport.pos.x) * windowScale, y});
        }

        // Update lander
        float dt = elapsed / 1000.f;
        bool engineon = controller.throttle.status();
        int rotation = 0;
        if (controller.left.status()) {
            rotation -= 1;
        }
        if (controller.right.status()) {
            rotation += 1;
        }
        lander.update(dt, engineon, rotation);

        // Lander
        device->SetIntensity(0.4f);

        // lander position
        auto rotatedPoint = [&](float x, float y) -> AudioRender::Point {
            auto v = glm::rotate(glm::vec2(x, y), lander.angle);
            // v += offset;
            return {v.x, v.y};
        };

        // TODO rotation point on the middle of mass?
        std::vector<AudioRender::Point> points;
        points.push_back(rotatedPoint(0, -lander.height / 2));
        points.push_back(rotatedPoint(-lander.width / 2, lander.height / 5));  // left
        points.push_back(rotatedPoint(0, 0));                                  // center
        points.push_back(rotatedPoint(lander.width / 2, lander.height / 5));   // right
        points.push_back(rotatedPoint(0, -lander.height / 2));

        // y coordinate border checks
        if (lander.pos.y < 0) {
            // TODO lose game
            lander.pos.y = 0;
        }
        if (lander.pos.y > viewport.height) {
            // TODO lose game
            lander.pos.y = (float)viewport.height;
        }

        bool collided = false;
        for (size_t k = 0; k < points.size(); k++) {
            float lp1x = points[k].x + lander.pos.x;
            float lp1y = points[k].y + lander.pos.y;
            float lp2x = points[(k + 1) % points.size()].x + lander.pos.x;
            float lp2y = points[(k + 1) % points.size()].y + lander.pos.y;

            int sx = (int)std::min(std::floorf(lp1x), std::floorf(lp2x));
            for (int i = std::max(0, sx - 4); i < std::min(sx + 4, (int)terrain.size() - 1) && !collided; i++) {
                float gp1x = (float)i;
                float gp1y = (float)terrain[i];
                float gp2x = (float)i + 1;
                float gp2y = (float)terrain[i + 1];

                auto ccw = [](float ax, float ay, float bx, float by, float cx, float cy) { return (cy - ay) * (bx - ax) > (by - ay) * (cx - ax); };

                auto intersect = [ccw](float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy) {
                    return ccw(ax, ay, cx, cy, dx, dy) != ccw(bx, by, cx, cy, dx, dy) && ccw(ax, ay, bx, by, cx, cy) != ccw(ax, ay, bx, by, dx, dy);
                };
                if (intersect(lp1x, lp1y, lp2x, lp2y, gp1x, gp1y, gp2x, gp2y)) collided = true;
            }
        }

        if (collided) {
            const int maxLandingAngle = 5;            // degrees
            const float maxHorisontalVelocity = 0.5;  // m/s
            const float maxVerticalVelocity = 1;      // m/s

            bool landed = false;
            // check if lander is on level
            if (std::abs(lander.angle) < DEGTORAD(maxLandingAngle)) {
                // check if on landing pad
                int lxs = (int)std::floorf(points[1].x + lander.pos.x);
                int lxe = (int)std::ceilf(points[3].x + lander.pos.x);
                for (auto& lp : landingPlaces) {
                    if (lp.first <= lxs && lp.second >= lxe) {
                        // at landing pad
                        // TODO check x and y velocity!
                        landed = true;
                    }
                }
            }


            if (!landed) {
                // Crash

                // F A I L
                drawLetter(l_f, {-8, -5});
                drawLetter(l_a, {-3, -5});
                drawLetter(l_i, {3, -5});
                drawLetter(l_l, {5, -5});

            } else {
                // Landing

                // O K
                // drawLetter(l_o, {-6, -5});
                // drawLetter(l_k, {2, -5});

                // W I N
                drawLetter(l_w, {-8, -5});
                drawLetter(l_i, {0, -5});
                drawLetter(l_n, {2, -5});
            }
        }

        // draw lander
        const Vector2Df centerOffset = lander.pos - viewport.pos;
        for (auto& p : points) {
            p.x += centerOffset.x;
            p.y += centerOffset.y;
        }

        device->SetPoint((points[0]) * windowScale);
        for (size_t i = 1; i < points.size(); i++) device->DrawLine(points[i] * windowScale);

        // DEBUG line
        // device->SetPoint({(-lander.width) / (float)windowWidth, 0.05f});
        // device->DrawLine({(+lander.height) / (float)windowWidth, 0.05f});

        if (engineon) {
            // draw engine exhaust
            device->SetIntensity(0.4f);

            // vary exhaust size
            float h = lander.height;
            h += (elapsed & 0x3) * h / 6.f;

            device->SetPoint(rotatedPoint(0, h) * windowScale);
            device->DrawLine(rotatedPoint(-lander.width / 4, lander.height / 4) * windowScale);
            device->SetPoint(rotatedPoint(0, h) * windowScale);
            device->DrawLine(rotatedPoint(lander.width / 4, lander.height / 4) * windowScale);
        }

        // TODO distance to ground should control zoom

        device->WaitSync();
        device->Submit();
    }
}  // namespace LunarLander

}  // namespace LunarLander
