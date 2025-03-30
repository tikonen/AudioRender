#pragma once

#include "DrawDevice.hpp"

struct NSVGimage;

namespace AudioRender
{
class SVGImage
{
public:
    ~SVGImage();

    bool loadImage(const char* filename);
    void drawImage(IDrawDevice* device, float scale = 1.0f, float stepSize = 5.f, float xoff = 0, float yoff = 0);

private:
    NSVGimage* m_image = NULL;
    IDrawDevice* m_device = NULL;
    float m_hw = 0;
    float m_hh = 0;
    float m_xoff = 0;
    float m_yoff = 0;

    void vertex(float x, float y, bool first);
    float m_xScale = 1.0f;
    float m_yScale = 1.0f;

    void drawPath(float* pts, int npts, char closed, float tol);
    void cubicBez(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, float tol, int level);
};
}  // namespace AudioRender
