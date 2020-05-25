#pragma once

namespace AudioRender
{
struct Rectangle {
    float left;
    float top;
    float right;
    float bottom;
};  // namespace Rectangle

struct Point {
    float x;
    float y;

    Point operator+(const Point&& p) const { return {x + p.x, y + p.y}; }
    Point operator*(float f) const { return {x * f, y * f}; }
};

class IDrawDevice
{
public:
    // Returns when all data has been submitted to audio pipeline and more is needed
    virtual bool WaitSync() = 0;

    // Call to start drawing
    virtual void Begin() = 0;

    // Call to end drawing and submit graphics for rendering
    virtual void Submit() = 0;

    // Graphics primitives
    virtual Rectangle GetViewPort() = 0;

    // set current draw point
    virtual void SetPoint(Point p) = 0;

    // set intensity of next draw
    virtual void SetIntensity(float intensity) = 0;

    // draw circle on current point
    virtual void DrawCircle(float radius) = 0;

    // draw line from current point to target point. Target point becomes new current point.
    // Line intensity will lerp linearnly towards intensity, if >= 0.
    virtual void DrawLine(Point to, float intensity = -1) = 0;

    // Move draw beam to point and let it stabilize (Results are very hardware dependent!)
    virtual void SyncPoint(Point to) = 0;
};

class DrawDevice : public IDrawDevice
{
public:
    const float DefaultIntensity = 0.5;

    //==========================================================
    // IDrawDevice interface
    void Begin() override;
    void SetPoint(Point p) override;
    void SetIntensity(float intensity) override;
    void DrawCircle(float radius) override;
    void DrawLine(Point to, float intensity = -1) override;
    void SyncPoint(Point to) override;
    Rectangle GetViewPort() override { return m_viewPort; }

protected:
    // Graphics operations
    struct GraphicsPrimitive {
        enum class Type { DRAW_CIRCLE, DRAW_LINE, DRAW_SYNC };

        Type type;
        float r;
        float intensity;
        float toIntensity;
        Point p;
        Point toPoint;
    };

    Point m_currPoint{0};
    float m_currIntensity = DefaultIntensity;
    std::vector<GraphicsPrimitive> m_operations;
    const Rectangle m_viewPort{-0.5, -0.5, 0.5, 0.5};
};
}  // namespace AudioRender
