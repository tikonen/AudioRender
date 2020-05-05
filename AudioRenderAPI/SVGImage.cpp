
#include <stdio.h>
#include <string.h>
#include <math.h>
#define NANOSVG_IMPLEMENTATION  // Expands implementation
#pragma warning(push)
#pragma warning(disable : 4244)  // conversion from 'double' to 'float', possible loss of data
#include <nanosvg.h>
#pragma warning(pop)

#include "SVGImage.hpp"

namespace AudioRender
{
SVGImage::~SVGImage()
{
    if (m_image) nsvgDelete(m_image);
    m_image = NULL;
}

bool SVGImage::loadImage(const char* filename)
{
    m_image = nsvgParseFromFile(filename, "px", 96);
    // if (m_image) {
    //    printf("size: %f x %f\n", m_image->width, m_image->height);
    //}
    return m_image != NULL;
}

void SVGImage::drawImage(IDrawDevice* device, float scale, float stepSize, float xoffset, float yoffset)
{
    if (!m_image) return;

    m_device = device;
    m_xScale = scale / (m_image->width);
    m_yScale = scale / (m_image->height);
    m_hw = m_image->width / 2;
    m_hh = m_image->height / 2;
    m_xoff = xoffset;
    m_yoff = yoffset;

    NSVGshape* shape = m_image->shapes;
    while (shape) {
        NSVGpath* path = shape->paths;
        while (path) {
            drawPath(path->pts, path->npts, path->closed, stepSize);
            path = path->next;
        }
        shape = shape->next;
    }
    m_device = NULL;
}

void SVGImage::vertex(float x, float y, bool first)
{
    const AudioRender::Point point{(x - m_hw) * m_xScale + m_xoff, (y - m_hh) * m_yScale + m_yoff};

    assert(m_device != NULL);
    if (first) {
        m_device->SetPoint(point);
    } else {
        m_device->DrawLine(point);
    }
}

static float distPtSeg(float x, float y, float px, float py, float qx, float qy)
{
    float pqx, pqy, dx, dy, d, t;
    pqx = qx - px;
    pqy = qy - py;
    dx = x - px;
    dy = y - py;
    d = pqx * pqx + pqy * pqy;
    t = pqx * dx + pqy * dy;
    if (d > 0) t /= d;
    if (t < 0)
        t = 0;
    else if (t > 1)
        t = 1;
    dx = px + t * pqx - x;
    dy = py + t * pqy - y;
    return dx * dx + dy * dy;
}

void SVGImage::cubicBez(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, float tol, int level)
{
    float x12, y12, x23, y23, x34, y34, x123, y123, x234, y234, x1234, y1234;
    float d;

    if (level > 12) return;

    x12 = (x1 + x2) * 0.5f;
    y12 = (y1 + y2) * 0.5f;
    x23 = (x2 + x3) * 0.5f;
    y23 = (y2 + y3) * 0.5f;
    x34 = (x3 + x4) * 0.5f;
    y34 = (y3 + y4) * 0.5f;
    x123 = (x12 + x23) * 0.5f;
    y123 = (y12 + y23) * 0.5f;
    x234 = (x23 + x34) * 0.5f;
    y234 = (y23 + y34) * 0.5f;
    x1234 = (x123 + x234) * 0.5f;
    y1234 = (y123 + y234) * 0.5f;

    d = distPtSeg(x1234, y1234, x1, y1, x4, y4);
    if (d > tol * tol) {
        cubicBez(x1, y1, x12, y12, x123, y123, x1234, y1234, tol, level + 1);
        cubicBez(x1234, y1234, x234, y234, x34, y34, x4, y4, tol, level + 1);
    } else {
        vertex(x4, y4, false);
    }
}

void SVGImage::drawPath(float* pts, int npts, char closed, float tol)
{
    int i;
    vertex(pts[0], pts[1], true);
    for (i = 0; i < npts - 1; i += 3) {
        float* p = &pts[i * 2];
        cubicBez(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], tol, 0);
    }
    if (closed) {
        vertex(pts[0], pts[1], false);
    }
}

}  // namespace AudioRender
