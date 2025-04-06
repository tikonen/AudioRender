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

#define G 1.625f  // m/s^2
#define DEGTORAD(d) ((float)M_PI / 180.f * (d))

namespace LunarLander
{
// Generic timer utility
struct Timer {
    const float interval;
    float cumulative = 0;
    int flipflop = 1;  // toggles state on every timer expiry
    const bool autoReset;

    Timer(bool autoRes, float t)
        : interval(t)
        , autoReset(autoRes)
    {
        reset();
    }

    void reset()
    {
        cumulative = 0;
        flipflop = 0;
    }

    bool update(float elapseds)
    {
        cumulative += elapseds;
        if (cumulative >= interval) {
            if (autoReset) {
                cumulative -= interval;
                cumulative = 0;
                flipflop = 1 - flipflop;
            }
            return true;
        }
        return false;
    }
};

struct Button {
    virtual ~Button() = default;
    // Button down status
    virtual bool status() = 0;

    // Button changed state to down on the last frame
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

// Utility to write text on the view
struct TextUtil {
    using Character = std::vector<std::vector<AudioRender::Point>>;
    
    const Character l_A = {{{0, 0}, {0, -7.5}, {1, -10}, {2.5, -10}, {3.5, -7.5}, {3.5, 0}}, {{0, -5}, {3.5, -5}}};
    const Character l_E = {{{3.5f, 0}, {0, 0}, {0, -10}, {3.5f, -10}}, {{0, -5.f}, {3, -5.f}}};
    const Character l_F = {{{0, 0}, {0, -10}, {4, -10}}, {{0, -5.f}, {3, -5.f}}};
    const Character l_I = {{{0, 0}, {0, -10}}};
    const Character l_K = {{{0, 0}, {0, -10}}, {{4, -10}, {0, -5}, {4, 0}}};
    const Character l_L = {{{0, -10}, {0, 0}, {3.5f, 0}}};
    const Character l_O = {{{3, 0}, {6, -5}, {3, -10}, {0, -5}, {3, 0}}};    
    const Character l_P = {{{0, 0}, {0, -10}, {1, -10}, {3.5, -9}, {3.5, -6}, {1, -5}, {0, -5}}};
    const Character l_R = {{{0, 0}, {0, -10}, {5, -7}, {0, -3}, {5, 0}}};
    const Character l_S = {{{3.5, -10}, {0, -10}, {0, -5}, {3.5, -5}, {3.5, 0}, {0, 0}}};
    const Character l_U = {{{0, -10}, {0, 0}, {3.5f, 0}, {3.5f, -10}}};
    const Character l_W = {{{0, -10}, {6.f / 3, 0}, {6.f / 2, -6}, {6.f - 6.f / 3, 0}, {6, -10}}};
    const Character l_N = {{{0, 0}, {0, -10}, {4, 0}, {4, -10}}};
    const Character l_D = {{{0, 0}, {0, -10}, {3.5, -8}, {3.5, -2}, {0, 0}}};
    const Character l__ = {{{0, 0}, {4, 0}}};

    const Character d0 = {{{0, 0}, {0, -10}, {4, -10}, {4, 0}, {0, 0}, {4, -10}}};
    const Character d1 = {{{2, 0}, {2, -10}, {1, -9}}};
    const Character d2 = {{{0, -10}, {4, -10}, {4, -5}, {0, -5}, {0, 0}, {4, 0}}};
    const Character d3 = {{{0, -10}, {4, -10}, {4, 0}, {0, 0}}, {{1, -5}, {4, -5}}};
    const Character d4 = {{{4, 0}, {4, -10}, {0, -4}, {5, -4}}};
    const Character d5 = {{{0, 0}, {4, 0}, {4, -5}, {0.5, -5}, {0.5, -10}, {4, -10}}};
    const Character d6 = {{{0, -5}, {4, -5}, {4, 0}, {0, 0}, {0, -10}, {4, -10}}};
    const Character d7 = {{{3, 0}, {4, -10}, {0, -10}}};
    const Character d8 = {{{0, 0}, {0, -10}, {4, -10}, {4, 0}, {0, 0}}, {{0, -5}, {4, -5}}};
    const Character d9 = {{{3, 0}, {4, -10}, {0, -10}, {0, -6}, {4, -6}}};

    const Character* digits[10] = {&d0, &d1, &d2, &d3, &d4, &d5, &d6, &d7, &d8, &d9};

    const float letterScale = 1.f / 10 * 0.2f;
    AudioRender::IDrawDevice* device;
    const Character* letters[0xFF + 1];

    TextUtil(AudioRender::IDrawDevice* drawDevice)
        : device(drawDevice)
    {
        for (int i = 0; i <= 0xFF; i++) {
            letters[i] = &l__;
        }
        letters['A'] = &l_A;
        letters['E'] = &l_E;
        letters['F'] = &l_F;
        letters['I'] = &l_I;
        letters['K'] = &l_K;
        letters['L'] = &l_L;
        letters['O'] = &l_O;
        letters['P'] = &l_P;
        letters['R'] = &l_R;
        letters['S'] = &l_S;
        letters['U'] = &l_U;
        letters['W'] = &l_W;
        letters['N'] = &l_N;
        letters['D'] = &l_D;
        letters['0'] = &d0;
        letters['1'] = &d1;
        letters['2'] = &d2;
        letters['3'] = &d3;
        letters['4'] = &d4;
        letters['5'] = &d5;
        letters['6'] = &d6;
        letters['7'] = &d7;
        letters['8'] = &d8;
        letters['9'] = &d9;
    }

    void drawCharacter(char c, float xpos, float ypos, float scale = 1.0f) { drawCharacter(letters[c], xpos, ypos, scale); };

    void drawDigit(int d, float xpos, float ypos, float scale = 1.0f) { drawCharacter(digits[d % 10], xpos, ypos, scale); };

    void writeText(const char* text, float xpos, float ypos, float scale = 1.0f)
    {
        static float widthCache[0xFF + 1] = {0};
        const float spacing = 2.f;

        while (char c = *text++) {
            const Character* cr = letters[c];

            float width = widthCache[c];
            if (width == 0.0f) {
                float minx = std::numeric_limits<float>::max();
                float maxx = std::numeric_limits<float>::min();
                for (auto& seg : *cr) {
                    for (auto& p : seg) {
                        minx = std::min(minx, p.x);
                        maxx = std::max(maxx, p.x);
                    }
                }
                width = widthCache[c] = std::max(std::abs(maxx - minx), 1.5f);
            }
            drawCharacter(cr, xpos, ypos, scale);
            xpos += width + spacing;
        }
    };

private:
    void drawCharacter(const Character* lf, float xpos, float ypos, float scale)
    {
        const AudioRender::Point offset{xpos, ypos};
        for (const auto& seg : *lf) {
            device->SetPoint((seg[0] + offset) * letterScale * scale);
            for (size_t i = 1; i < seg.size(); i++) {
                device->DrawLine((seg[i] + offset) * letterScale * scale);
            }
        }
    };
};

// Vector implementation
using Vector2D = glm::ivec2;
using Vector2Df = glm::vec2;

static inline Vector2Df vrotate(const Vector2Df& v, float rads) { return glm::rotate(v, rads); }

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
    Key quit = Key(0x51);   // Q

    // DEBUG
    Key nextLevel = Key(0x4E);  // 'N'

    std::vector<Key*> keys;

    Controller()
    {
        printf(
            "Arrow Left - Rotate left\n"
            "Arrow Right - Rotate Right\n"
            "Space Bar - Thrust\n"
            "P - Pause Game\n"
            "Q - Quit Game\n"
            "Arrow Up - Zoom In\n"
            "Arrow Down - Zoom Out\n"
            "R - Reset Game\n");

        keys.push_back(&left);
        keys.push_back(&right);
        keys.push_back(&throttle);
        keys.push_back(&zoomIn);
        keys.push_back(&zoomOut);
        keys.push_back(&pause);
        keys.push_back(&reset);
        keys.push_back(&quit);
    };

    void update()
    {
        for (auto* key : keys) {
            key->update();
        }
    }
};

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
    const float mass = 100.f;  // in fuel units
    const float thrust = 3 * G * (mass * 1.5f);
    const float angularAcc = (mass * 1.5f) * DEGTORAD(40);  // d/s^2
    const Vector2Df initialVelocity = {3, 0};
    const float initialFuel = 100.f;

    void update(float dt, int& engine, int rotation)
    {
        // lander rotation
        const float rotationDampening = 0.80f;

        float adelta = angularAcc * rotation * dt / (mass + fuel);
        // Integrate rotation
        angle += (angularSpeed + adelta / 2.f) * dt;
        angularSpeed += adelta;

        // dampen the angular speed
        if (!rotation) angularSpeed -= angularSpeed * rotationDampening * dt;

        if (angle > (float)M_PI) angle = -(2 * (float)M_PI - angle);
        if (angle < (float)-M_PI) angle = (2 * (float)M_PI + angle);

        // lander position
        const float accScale = 1 / 2.f;

        if (engine && fuel > 0) {
            fuel -= dt;
            if (fuel < 0) {
                fuel = 0;
            }
        } else {
            engine = 0;
        }
        Vector2Df tvec = Vector2Df(0, engine * -thrust / (mass + fuel));
        Vector2Df totalAcc = A + vrotate(tvec, angle);
        // Integrate position
        Vector2Df vdelta = totalAcc * dt * accScale;
        pos += (velocity + vdelta / 2.f) * dt;
        velocity += vdelta;
    }

    void nextLevel(float posx, float posy, float fuelRefund)
    {
        float f = fuel;
        reset(posx, posy);
        const float minimumFuel = 5.f;
        fuel = f + initialFuel * fuelRefund;
        if (fuel < minimumFuel) fuel = minimumFuel;
    }

    void reset(float posx, float posy)
    {
        velocity = initialVelocity;
        angularSpeed = 0;
        angle = 0;
        fuel = initialFuel;
        pos = {posx, posy};
    }
};


void Game::mainLoop(std::atomic_bool& running, AudioRender::IDrawDevice* device)
{
    ViewPort viewport;
    viewport.reset();

    Controller controller;
    struct TextUtil textUtil(device);

    Map map;

    auto generateLevel = [&](int level) { map = generateTerrain(level, viewport.width); };

    Timer coolDownTimer(false, 2.0f);  // Restrics state change speed
    enum GameState { ST_WAIT, ST_PLAY, ST_WIN, ST_FAIL } gameState = ST_WAIT;
    float totalTime = 0;
    int paused = 0;
    int level = 0;
    generateLevel(level);

    Lander lander;

    auto updateLanderPosition = [&](Vector2Df& pos, float minheight) {
        int xs = (int)std::floorf(pos.x - lander.width - 20);
        const int xe = (int)std::ceilf(pos.x + lander.width + 20);
        // ensure some free space between terrain and lander
        for (; xs < xe; xs++) {
            int y = map.terrain[xs];
            if (y < pos.y + minheight) {
                pos.y = y - minheight;
            }
        }

        if (map.terrain[(int)pos.x] > pos.y + minheight * 10) {
            pos.y = map.terrain[(int)pos.x] - minheight * 10;
        }
    };

    lander.reset(viewport.pos.x, 50);
    updateLanderPosition(lander.pos, 10);

    while (running) {
        using namespace std::chrono;
        static auto ticks = system_clock::now();
        auto now = system_clock::now();
        long long elapsed = duration_cast<milliseconds>(now - ticks).count();
        ticks = now;

        // Controller
        controller.update();

        if (controller.quit.pressed()) {
            running = false;
        }

        if (controller.pause.pressed()) {
            paused = !paused;
        }
        totalTime += elapsed;

        /*
        {
            static Timer timer(true, 1.f);
            static int num = 0;
            device->Begin();
            device->SetIntensity(0.2f);

            float offset = 0;
            int digit = num;
            do {
                drawCharacter(digits[digit % 10], {offset, -5});
                offset -= 5.5f;
                digit /= 10;
            } while (digit > 0);
            drawCharacter(l_l, {offset, -5});

            if (timer.update(elapsed / 1000.f)) num++;


            device->WaitSync();
            device->Submit();
            continue;
        }
        */

        if (paused) {
            if (paused == 1) {
                textUtil.writeText("PAUSED", -10, -10.0f, .5f);
                paused = 2;
            }
            // just show last render
            running = running && device->WaitSync(1000);
            device->Submit();
            continue;
        }
        // clear frame
        device->Begin();
        device->SetIntensity(0.25f);

        if (controller.zoomIn.pressed()) {
            viewport.zoom += 1.f;
        }
        if (controller.zoomOut.pressed()) {
            viewport.zoom -= 1.f;
        }

        auto resetGame = [&]() {
            level = 0;
            generateLevel(level);
            viewport.reset();
            lander.reset(viewport.pos.x, 50);
            updateLanderPosition(lander.pos, 10);
            gameState = ST_WAIT;
            coolDownTimer.reset();
            totalTime = 0;
        };

        auto advanceLevel = [&](float fuelBonus) {
            level++;
            generateLevel(level);
            viewport.reset();
            lander.nextLevel(viewport.pos.x + ((level & 1) ? 100 : -100), 50, fuelBonus);
            updateLanderPosition(lander.pos, 10);
            if (level & 1) lander.velocity.x *= -1;
            gameState = ST_WAIT;
            coolDownTimer.reset();
        };

        if (controller.reset.pressed()) {
            // reset game state
            resetGame();
        }

        if (controller.nextLevel.pressed()) {
            advanceLevel(0.0f);
        }

        if (gameState == ST_WAIT) {
            // Level
            float offset = 0;
            int digit = level;
            do {
                textUtil.drawDigit(digit % 10, offset, -5);
                offset -= 5.5f;
                digit /= 10;
            } while (digit > 0);
            textUtil.drawCharacter('L', offset, -5);

            if (controller.throttle.pressed() || controller.left.pressed() || controller.right.pressed()) {
                gameState = ST_PLAY;
            }
        } else if (gameState == ST_WIN) {
            // W I N
            textUtil.writeText("WIN", -8, -5.2f);

            if (coolDownTimer.update(elapsed / 1000.f)) {
                if (controller.throttle.pressed()) {
                    // advance to next level
                    advanceLevel(1 / 4.f);
                }
            }
        } else if (gameState == ST_FAIL) {
            // F A I L
            textUtil.writeText("FAIL", -8, -5.2f);

            if (coolDownTimer.update(elapsed / 1000.f)) {
                if (controller.throttle.pressed()) {
                    if (lander.fuel > 0) {
                        // advance to next level
                        advanceLevel(-1 / 4.f);
                    } else {
                        // Game over
                        resetGame();
                    }
                }
            }
        }

        // Viewport
        viewport.zoom = std::max(viewport.zoom, 1.f);
        viewport.zoom = std::min(viewport.zoom, 15.f);

        viewport.pos.x = lander.pos.x;
        viewport.pos.y = lander.pos.y;

        const int windowWidth = std::lroundf(viewport.width / viewport.zoom);
        const float windowScale = 1.f / windowWidth;

        // Get visible terrain borders
        int terrainxs = viewport.terrainPos.x + std::lroundf(viewport.pos.x) - windowWidth / 2;

        // Enforce borders
        if (terrainxs < 0) {
            viewport.pos.x = windowWidth / 2.f;
            terrainxs = 0;
        }
        if (terrainxs + windowWidth > map.terrain.size()) {
            terrainxs = (int)map.terrain.size() - windowWidth;
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
            if (terrainxe > map.terrain.size()) {
                terrainxe = (int)map.terrain.size();
            }

            float terrainyOffset = viewport.pos.y - viewport.terrainPos.y;

            // Draw terrain
            int xs = terrainxs;

            float y0 = (map.terrain[xs] - terrainyOffset) * windowScale;
            xs += step;
            bool newSeg = true;
            for (; xs < terrainxe; xs += step) {
                float y1 = (map.terrain[xs] - terrainyOffset) * windowScale;
                if (std::fabsf(y0) > 0.55f && std::fabs(y1) > 0.55f) {  // not visible segment
                    y0 = y1;
                    newSeg = true;
                    continue;
                }
                if (newSeg) {
                    device->SetPoint({(xs - step - viewport.pos.x) * windowScale, y0});
                    newSeg = false;
                }
                const float x = (xs - viewport.pos.x) * windowScale;
                device->DrawLine({x, y1});
                y0 = y1;
            }

            // Mark landing places
            device->SetIntensity(0.5f);
            for (auto& p : map.landingPlaces) {
                if (p.first > terrainxe) continue;
                if (p.second < terrainxs) continue;

                const float y = (map.terrain[p.first] - terrainyOffset + 1) * windowScale;
                if (std::fabsf(y) > 0.55f) continue;  // not visible

                device->SetPoint({(p.first - viewport.pos.x) * windowScale, y});
                device->DrawLine({(p.second - viewport.pos.x) * windowScale, y});
            }
        }

        // Lander
        int engineon = controller.throttle.status();
        if (gameState == ST_PLAY || gameState == ST_WAIT) {
            int rotation = 0;
            if (controller.left.status()) {
                rotation -= 1;
            }
            if (controller.right.status()) {
                rotation += 1;
            }

            lander.update(elapsed / 1000.f, engineon, rotation);
        } else {
            engineon = false;
        }

        device->SetIntensity(1.0f);

        // lander position
        auto rotatedPoint = [&](float x, float y) -> AudioRender::Point {
            auto v = vrotate(Vector2Df(x, y), lander.angle);
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
            // Check for collisions

            // Game area border checks
            if (lander.pos.y < -50 || (lander.pos.y >= viewport.height) || lander.pos.x < 0 || lander.pos.x >= viewport.width) {
                gameState = ST_FAIL;
            }

            bool collided = false;
            // Check intersection for each lander line segment with nearby terrain segments
            for (size_t k = 0; k < points.size(); k++) {
                float lp1x = points[k].x + lander.pos.x;
                float lp1y = points[k].y + lander.pos.y;
                float lp2x = points[(k + 1) % points.size()].x + lander.pos.x;
                float lp2y = points[(k + 1) % points.size()].y + lander.pos.y;

                int sx = (int)std::min(std::floorf(lp1x), std::floorf(lp2x));
                for (int i = std::max(0, sx - 4); i < std::min(sx + 4, (int)map.terrain.size() - 1) && !collided; i++) {
                    float gp1x = (float)i;
                    float gp1y = (float)map.terrain[i];
                    float gp2x = (float)i + 1;
                    float gp2y = (float)map.terrain[i + 1];

                    auto ccw = [](float ax, float ay, float bx, float by, float cx, float cy) { return (cy - ay) * (bx - ax) > (by - ay) * (cx - ax); };
                    auto intersect = [ccw](float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy) {
                        return ccw(ax, ay, cx, cy, dx, dy) != ccw(bx, by, cx, cy, dx, dy) && ccw(ax, ay, bx, by, cx, cy) != ccw(ax, ay, bx, by, dx, dy);
                    };
                    if (intersect(lp1x, lp1y, lp2x, lp2y, gp1x, gp1y, gp2x, gp2y)) collided = true;
                }
            }

            if (collided) {
                // Lander has collided with ground, determine if this was an acceptable landing

                // Requirements for landing
                const int maxLandingAngle = 5;             // degrees
                const float maxHorisontalVelocity = 0.8f;  // m/s
                const float maxVerticalVelocity = 1.2f;    // m/s

                bool landed = false;
                // check if lander is on level
                if (std::abs(lander.angle) < DEGTORAD(maxLandingAngle)) {
                    // check if on landing pad
                    int lxs = (int)std::floorf(points[1].x + lander.pos.x);
                    int lxe = (int)std::ceilf(points[3].x + lander.pos.x);
                    for (auto& lp : map.landingPlaces) {
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

            if (engineon) {
                // draw engine exhaust
                device->SetIntensity(0.1f);

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
        }

        // indicate fuel        
        if (gameState == ST_PLAY) {
            const float lowFuelAlarmLevel = .15f;

            float p = lander.fuel / lander.initialFuel;
            if (p > 0) {
                static Timer blinkTimer(true, 0.3f);
                blinkTimer.update(elapsed / 1000.f);
                int step = 1;
                if (p > lowFuelAlarmLevel || blinkTimer.flipflop) {  // blink if low fuel
#if 1
                    char buffer[16];
                    snprintf(buffer, sizeof(buffer), "%02d", (int)std::roundf(p * 100));
                    textUtil.writeText(buffer, -8, -35.0f, .5f);
#else
                    Vector2Df needle(-0.5, 0);
                    const int steps = 20;
                    needle = vrotate(needle, DEGTORAD(45 * p));
                    const float angleStep = DEGTORAD(90.f / steps);
                    device->SetPoint({needle.x, -needle.y});
                    for (float r = 0; r <= p; r += 1.f / steps) {
                        needle = vrotate(needle, -angleStep);
                        if (step)
                            device->DrawLine({needle.x, -needle.y});
                        else
                            device->SetPoint({needle.x, -needle.y});
                        step = 1 - step;
                    }
#endif
                }
            }            
        }
        


#if 0
        // check distance to ground and automatically update zoom
        {
            int x = (int)std::roundf(lander.pos.x);
            if (x < 0) x = 0;
            if (x > terrain.size()) x = (int)terrain.size() - 1;
            float dis = std::fabsf(lander.pos.y - terrain[x]);

            static Timer lastSwitch(false, 2.f);
            if (lastSwitch.update(elapsed / 1000.f)) {  // don't change zoom too often
                viewport.zoom = std::roundf(100.0f / dis);
                lastSwitch.reset();
            }
        }
#endif

        // Pass scene to rendering
        running = running && device->WaitSync(1000);
        device->Submit();
    }
}
}  // namespace LunarLander
