#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <vector>
#include <array>
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

    using Vector2D = glm::ivec2;
    using Vector2Df = glm::vec2;

    struct ViewPort {
        int width;
        int height;
        Vector2Df pos;
        const Vector2D terrainPos = {0, 0};
        float zoom;

        void reset()
        {
            height = 400;
            width = 400;
            pos = {200.f, 150.f};
            zoom = 3;
        }
    } viewport;
    viewport.reset();

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
        Key throttle = Key(VK_SPACE);
        // Key down = Key(VK_DOWN);
        Key zoomIn = Key(VK_UP);
        Key zoomOut = Key(VK_DOWN);
        // Key zoomIn = Key(0x5A);   // Z
        // Key zoomOut = Key(0x58);  // X
        Key pause = Key(0x50);  // P
        Key reset = Key(0x52);  // R

        Controller()
        {
            printf(
                "Arrow Left - Rotate left\n"
                "Arrow Right - Rotate Right\n"
                "Space Bar - Thrust\n"
                "P - Pause Game\n"
                "Arrow Up - Zoom In\n"
                "Arrow Down - Zoom Out\n"
                "R - Reset Game\n");
        };

        void update()
        {
            left.update();
            right.update();
            throttle.update();
            zoomIn.update();
            zoomOut.update();
            pause.update();
            reset.update();
        }

    } controller;

    struct Lander {
        const Vector2Df A = {0, G};
        Vector2Df velocity;
        Vector2Df pos;
        float angularSpeed;
        float angle;  // in radians
        float fuel;   // burntime in seconds

        const float height = 5.f;
        const float width = 5.f;

        Lander() { reset(0, 0); }

        // Parameters
        const float thrust = 3 * G;
#define DEGTORAD(d) ((float)M_PI / 180.f * (d))
        const float angularAcc = DEGTORAD(40);  // d/s^2
        const Vector2Df initialVelocity = {3, 0};
        const float initialFuel = 60.f;
        const float mass = 100.f;  // in fuel units

        void update(float t, int& engine, int rotation)
        {
            // lander rotation
            const float dampening = 0.80f;

            float adelta = angularAcc * rotation * t * mass / (mass + fuel);
            angle += (angularSpeed + adelta / 2.f) * t;
            angularSpeed += adelta;
            if (!rotation) angularSpeed -= angularSpeed * dampening * t;
            if (angle > (float)M_PI) angle = -(2 * (float)M_PI - angle);
            if (angle < (float)-M_PI) angle = (2 * (float)M_PI + angle);

            // lander position
            const float accScale = 1 / 2.f;

            if (engine && fuel > 0) {
                fuel -= t;
                if (fuel < 0) {
                    fuel = 0;
                }
            } else {
                engine = 0;
            }
            Vector2Df tvec = Vector2Df(0, engine * -thrust * mass / (mass + fuel));
            Vector2Df totalAcc = A + glm::rotate(tvec, angle);

            Vector2Df vdelta = totalAcc * t * accScale;
            pos += (velocity + vdelta / 2.f) * t;
            velocity += vdelta;
        }

        void nextLevel(float posx, float posy, float fuelRefund)
        {
            float f = fuel;
            reset(posx, posy);
            fuel = f * fuelRefund;
        }

        void reset(float posx, float posy)
        {
            velocity = initialVelocity;
            angularSpeed = 0;
            angle = 0;
            fuel = initialFuel;
            pos = {posx, posy};
        }

    } lander;

    lander.reset(viewport.pos.x, 50);

    enum GameState { ST_WAIT, ST_PLAY, ST_WIN, ST_FAIL } gameState;
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
    const Letter l_d = {{{0, 0}, {0, -10}, {4, -8}, {4, -2}, {0, 0}}};

    const Letter d0 = {{{0, 0}, {0, -10}, {4, -10}, {4, 0}, {0, 0}, {4, -10}}};
    const Letter d1 = {{{2, 0}, {2, -10}, {1, -9}}};
    const Letter d2 = {{{0, -10}, {4, -10}, {4, -5}, {0, -5}, {0, 0}, {4, 0}}};
    const Letter d3 = {{{0, -10}, {4, -10}, {4, 0}, {0, 0}}, {{1, -5}, {4, -5}}};
    const Letter d4 = {{{4, 0}, {4, -10}, {0, -4}, {5, -4}}};
    const Letter d5 = {{{0, 0}, {4, 0}, {4, -5}, {0.5, -5}, {0.5, -10}, {4, -10}}};
    const Letter d6 = {{{0, -5}, {4, -5}, {4, 0}, {0, 0}, {0, -10}, {4, -10}}};
    const Letter d7 = {{{4, 0}, {4, -10}, {0, -10}}};
    const Letter d8 = {{{0, 0}, {0, -10}, {4, -10}, {4, 0}, {0, 0}}, {{0, -5}, {4, -5}}};
    const Letter d9 = {{{3, 0}, {4, -10}, {0, -10}, {0, -6}, {4, -6}}};

    const Letter digits[10] = {d0, d1, d2, d3, d4, d5, d6, d7, d8, d9};

    float letterScale = 1.f / 10 * 0.2f;
    auto drawLetter = [&](const Letter& lf, AudioRender::Point offset) {
        for (const auto& seg : lf) {
            device->SetPoint((seg[0] + offset) * letterScale);
            for (size_t i = 1; i < seg.size(); i++) {
                device->DrawLine((seg[i] + offset) * letterScale);
            }
        }
    };

    LunarLander::Map map;
    std::vector<int> terrain;
    std::vector<std::pair<int, int>> landingPlaces;

    auto generateLevel = [&](int level) {
        map = generateTerrain(level, 400);
        terrain = map.terrain;
        landingPlaces = map.landingPlaces;
    };

    // Restrics state change speed
    float coolDownTimer = 0;
    int level = 0;
    generateLevel(level);

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

        /*
        {
            static float timer = 0;
            static int num = 0;
            device->Begin();
            device->SetIntensity(0.2f);

            float offset = 0;
            int digit = num;
            do {
                drawLetter(digits[digit % 10], {offset, -5});
                offset -= 5.5f;
                digit /= 10;
            } while (digit > 0);
            drawLetter(l_l, {offset, -5});

            timer += elapsed / 1000.f;
            if (timer > 1) {
                num++;
                timer = 0;
            }

            device->WaitSync();
            device->Submit();
            continue;
        }
        */


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
            viewport.zoom += 1.f;
        }
        if (controller.zoomOut.pressed()) {
            viewport.zoom -= 1.f;
        }
        /*
        if (controller.left.status()) {
            // viewport.pos.x = std::max(viewport.pos.x - 1, 0.f);
        }
        if (controller.right.status()) {
            // viewport.pos.x = std::min(viewport.pos.x + 1, (float)viewport.width);
        }
        if (controller.throttle.status()) {
            // viewport.pos.y = std::max(viewport.pos.y - 1, 0);
        }
        */

        if (controller.reset.pressed()) {
            // reset game state
            level = 0;
            generateLevel(level);
            viewport.reset();
            lander.reset(viewport.pos.x, 50);
            gameState = ST_WAIT;
            coolDownTimer = 0;
        }

        if (gameState == ST_WAIT) {
            static float timer = 0;
            static int blink = 1;
            float e = elapsed / 1000.f;
            timer += e;
            if (timer > .5f) {
                timer = 0;
                blink = 1 - blink;
            }
            blink = 1;
            if (blink) {
                // L A N D
                /*
                drawLetter(l_l, {-11, -5});
                drawLetter(l_a, {-6, -5});
                drawLetter(l_n, {0, -5});
                drawLetter(l_d, {6, -5});
                */
                // Level
                float offset = 0;
                int digit = level;
                do {
                    drawLetter(digits[digit % 10], {offset, -5});
                    offset -= 5.5f;
                    digit /= 10;
                } while (digit > 0);
                drawLetter(l_l, {offset, -5});
            }

            if (controller.throttle.pressed() || controller.left.pressed() || controller.right.pressed()) {
                gameState = ST_PLAY;
            }
        } else if (gameState == ST_WIN) {
            // W I N
            drawLetter(l_w, {-8, -5});
            drawLetter(l_i, {0, -5});
            drawLetter(l_n, {2, -5});

            coolDownTimer += elapsed / 1000.f;
            if (coolDownTimer > 2) {
                if (controller.throttle.pressed()) {
                    // next level
                    level++;
                    generateLevel(level);
                    viewport.reset();
                    // give some extra fuel
                    lander.nextLevel(viewport.pos.x, 50, 1.3f);
                    if (level & 1) lander.velocity.x *= -1;
                    gameState = ST_WAIT;
                    coolDownTimer = 0;
                }
            }
        } else if (gameState == ST_FAIL) {
            // F A I L
            drawLetter(l_f, {-8, -5});
            drawLetter(l_a, {-3, -5});
            drawLetter(l_i, {3, -5});
            drawLetter(l_l, {5, -5});

            coolDownTimer += elapsed / 1000.f;
            if (coolDownTimer > 2) {
                if (controller.throttle.pressed()) {
                    if (lander.fuel > 0) {
                        level++;
                        generateLevel(level);
                        viewport.reset();
                        // fuel penalty
                        lander.nextLevel(viewport.pos.x, 50, 0.8f);
                    } else {
                        level = 0;
                        generateLevel(level);
                        viewport.reset();
                        lander.reset(viewport.pos.x, 50);
                    }
                    if (level & 1) lander.velocity.x *= -1;
                    gameState = ST_WAIT;
                    coolDownTimer = 0;
                }
            }
        }

        viewport.zoom = std::max(viewport.zoom, 1.f);
        viewport.zoom = std::min(viewport.zoom, 15.f);

        viewport.pos.x = lander.pos.x;
        viewport.pos.y = lander.pos.y;

        const int windowWidth = std::lroundf(viewport.width / viewport.zoom);
        const float windowScale = 1.f / windowWidth;

        // Get visible terrain borders
        int terrainxs = viewport.terrainPos.x + std::lroundf(viewport.pos.x) - windowWidth / 2;

        if (terrainxs < 0) {
            viewport.pos.x = windowWidth / 2.f;
            terrainxs = 0;
        }
        if (terrainxs + windowWidth > terrain.size()) {
            terrainxs = (int)terrain.size() - windowWidth;
            viewport.pos.x = viewport.width - windowWidth / 2.f;
        }

        // Terrain
        {
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
        }

        // Update lander
        float dt = elapsed / 1000.f;
        int engineon = controller.throttle.status();
        if (gameState == ST_PLAY || gameState == ST_WAIT) {
            int rotation = 0;
            if (controller.left.status()) {
                rotation -= 1;
            }
            if (controller.right.status()) {
                rotation += 1;
            }

            lander.update(dt, engineon, rotation);
        } else {
            engineon = false;
        }

        // Lander
        device->SetIntensity(0.4f);

        // lander position
        auto rotatedPoint = [&](float x, float y) -> AudioRender::Point {
            auto v = glm::rotate(glm::vec2(x, y), lander.angle);
            // v += offset;
            return {v.x, v.y};
        };

        // TODO move rotation point on the middle of mass?
        std::vector<AudioRender::Point> points;
        if (gameState == ST_FAIL) {
            // Crashed lander
            points.push_back(rotatedPoint(0.5f, -lander.height / 2 - 0.2f));
            points.push_back(rotatedPoint(-0.5f - lander.width / 2, lander.height / 5 + 0.5f));  // left
            points.push_back(rotatedPoint(0, 0));                                                // center
            points.push_back(rotatedPoint(lander.width / 2 + 0.1f, 2.f - lander.height / 5));    // right
            points.push_back(rotatedPoint(0.4f, -1.f - lander.height / 2));
        } else {
            // Pristine lander
            points.push_back(rotatedPoint(0, -lander.height / 2));
            points.push_back(rotatedPoint(-lander.width / 2, lander.height / 5));  // left
            points.push_back(rotatedPoint(0, 0));                                  // center
            points.push_back(rotatedPoint(lander.width / 2, lander.height / 5));   // right
            points.push_back(rotatedPoint(0, -lander.height / 2));
        }

        if (gameState == ST_PLAY || gameState == ST_WAIT) {
            // Checke for collisions

            // y coordinate border checks
            if (lander.pos.y < -50) {
                lander.pos.y = -50;
                gameState = ST_FAIL;
            }
            if (lander.pos.y > viewport.height) {
                lander.pos.y = (float)viewport.height;
                gameState = ST_FAIL;
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
                // Lander has collided with ground, determine if this was an acceptable landing

                const int maxLandingAngle = 5;             // degrees
                const float maxHorisontalVelocity = 0.8f;  // m/s
                const float maxVerticalVelocity = 1.2f;    // m/s

                bool landed = false;
                // check if lander is on level
                if (std::abs(lander.angle) < DEGTORAD(maxLandingAngle)) {
                    // check if on landing pad
                    int lxs = (int)std::floorf(points[1].x + lander.pos.x);
                    int lxe = (int)std::ceilf(points[3].x + lander.pos.x);
                    for (auto& lp : landingPlaces) {
                        if (lp.first <= lxs && lp.second >= lxe) {
                            // at landing pad
                            if (std::fabsf(lander.velocity.x) <= maxHorisontalVelocity && std::fabsf(lander.velocity.y) <= maxVerticalVelocity) {
                                landed = true;
                            } else {
                                // too hard landing
                            }
                        }
                    }
                }

                if (!landed) {
                    // Crash
                    gameState = ST_FAIL;

                } else {
                    // Successful landing
                    gameState = ST_WIN;
                }
            }
        }

        // draw lander
        {
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

                std::array<AudioRender::Point, 4> exhaust = {            //
                    rotatedPoint(0, h),                                  //
                    rotatedPoint(-lander.width / 4, lander.height / 4),  //
                    rotatedPoint(0, h),                                  //
                    rotatedPoint(lander.width / 4, lander.height / 4)};
                for (auto& p : exhaust) {
                    p.x += centerOffset.x;
                    p.y += centerOffset.y;
                }

                device->SetPoint(exhaust[0] * windowScale);
                device->DrawLine(exhaust[1] * windowScale);
                device->SetPoint(exhaust[2] * windowScale);
                device->DrawLine(exhaust[3] * windowScale);
            }

            // indicate fuel
            {
                const float lowFuelAlarmLevel = .15f;

                float p = lander.fuel / lander.initialFuel;
                if (p > 0) {
                    static float timer = 0;
                    static int blink = 1;
                    float e = elapsed / 1000.f;
                    timer += e;
                    if (timer > .3f) {
                        timer = 0;
                        blink = 1 - blink;
                    }
                    if (p > lowFuelAlarmLevel || blink) {  // blink if low fuel
                        glm::vec2 needle(-0.5, 0);
                        const int steps = 20;
                        needle = glm::rotate(needle, DEGTORAD(45 * p));
                        const float angleStep = DEGTORAD(90.f / steps);
                        device->SetPoint({needle.x, -needle.y});
                        for (float r = 0; r <= p; r += 1.f / steps) {
                            needle = glm::rotate(needle, -angleStep);
                            device->DrawLine({needle.x, -needle.y});
                        }
                    }
                }
            }
        }

#if 0
        // check distance to ground and update zoom
        {
            int x = (int)std::roundf(lander.pos.x);
            if (x < 0) x = 0;
            if (x > terrain.size()) x = (int)terrain.size() - 1;
            float dis = std::fabsf(lander.pos.y - terrain[x]);

            static float lastSwitch = 0;
            lastSwitch += elapsed;
            if (lastSwitch > 3.f) {  // don't change zoom too often
                viewport.zoom = std::roundf(100.0f / dis);
                lastSwitch = 0.0f;
            }
        }
#endif

        // Pass scene to rendering
        device->WaitSync();
        device->Submit();
    }
}
}  // namespace LunarLander
