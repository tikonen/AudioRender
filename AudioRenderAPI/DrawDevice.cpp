#include "pch.h"

#include "DrawDevice.hpp"

namespace AudioRender
{
void DrawDevice::Begin()
{
    m_operations.clear();
    m_currIntensity = DefaultIntensity;
    m_currPoint = Point{0};
}

void DrawDevice::DrawCircle(float radius)
{
    GraphicsPrimitive op{GraphicsPrimitive::Type::DRAW_CIRCLE, radius, m_currIntensity, m_currPoint, m_currPoint};
    m_operations.emplace_back(op);
}

void DrawDevice::DrawLine(Point to, float intensity)
{
    float fromIntensity = m_currIntensity;
    if (intensity >= 0) m_currIntensity = intensity;
    Point fromPoint = m_currPoint;
    m_currPoint = to;
    GraphicsPrimitive op{GraphicsPrimitive::Type::DRAW_LINE, -1, fromIntensity, fromPoint, m_currPoint};
    m_operations.emplace_back(op);
}

void DrawDevice::SetIntensity(float intensity) { m_currIntensity = intensity; }

void DrawDevice::SetPoint(Point p)
{
    m_currPoint = p;
    GraphicsPrimitive op{GraphicsPrimitive::Type::DRAW_SYNC, -1, m_currIntensity,  m_currPoint, m_currPoint};
    m_operations.emplace_back(op);
}

}  // namespace AudioRender
