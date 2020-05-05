#include "pch.h"

#include <vector>

#include "DrawDevice.hpp"
#include "RasterImage.hpp"

#include "Log.hpp"

#include <stb_image.h>
#include <stb_image_write.h>
#include <stb_image_resize.h>

namespace AudioRender
{
bool RasterImage::loadImage(const char* filename)
{
    int width, height, channels;
    FILE* f;
    if (fopen_s(&f, filename, "rb") == 0) {
        stbi_uc* data = stbi_load_from_file(f, &width, &height, &channels, 1);
        fclose(f);

        if (!data) {
            LOGE("Image loading failed. %s", stbi_failure_reason());
            return false;
        }
        bool ret = processImageData(data, width, height);
        stbi_image_free(data);
        return ret;
    }
    LOGE("Cannot open file %s\n. %s", filename, GetLastErrorString());
    return false;
}


bool RasterImage::loadImage(const uint8_t* buffer, int len)
{
    int width, height, channels;
    stbi_uc* data = stbi_load_from_memory(buffer, len, &width, &height, &channels, 1);
    if (!data) {
        LOGE("Image loading failed. %s", stbi_failure_reason());
        return false;
    }
    bool ret = processImageData(data, width, height);
    stbi_image_free(data);
    return ret;
}

bool RasterImage::processImageData(const uint8_t* buffer, int width, int height)
{
    // normalize image size
    m_height = m_width = 70;
    /*
    if (width > height) {
        m_height = std::lround(m_width / aspect);
    } else if (height > width) {
        m_width = std::lround(m_height * aspect);
    }
    */

    std::vector<unsigned char> image(m_width * m_height);
    stbir_resize_uint8(buffer, width, height, 0, image.data(), m_width, m_height, 0, 1);
    m_scanLines.clear();
    m_scanLines.resize(m_height);
    // stbi_write_png("out.png", m_width, m_height, 1, image.data(), 0);

    const unsigned char blackLevel = 0x0A;
    const unsigned char whiteLevel = 0xF0;

    // Convert to centered rasterized lines
    for (int y = 0; y < m_height; y++) {
        int ps = -1;
        for (int x = 0; x < m_width; x++) {
            // unsigned char pixel0 = image[(y - 1) * m_width + x];
            unsigned char pixel = image[y * m_width + x];
            // unsigned char pixel1 = image[(y + 1) * m_width + x];
            // pixel = (pixel0 + pixel + pixel1) / 3;
            if (pixel <= blackLevel) {  // black
                if (ps == -1) ps = x;   // line starts

            } else if (pixel >= whiteLevel) {
                if (ps != -1 && x - ps > 1) {
                    // line ends
                    m_scanLines[y].emplace_back(ps - m_width / 2, x - m_width / 2);
                    ps = -1;
                }
            }
        }
        if (ps != -1 && m_width - ps > 1) {
            m_scanLines[y].emplace_back(ps - m_width / 2, m_width - m_width / 2);
        }
    }
    return true;
}
void RasterImage::drawImage(IDrawDevice* device)
{
    const float Amplitude = 0.7f;
    const int rowCount = (int)m_scanLines.size();

    int flipflop = 0;

    for (int row = 0; row < rowCount; row++) {
        const float y = (row - rowCount / 2) / float(rowCount / 2) * Amplitude / 2;
        const auto& lines = m_scanLines[row];

        if (lines.size() == 0) continue;

        if (flipflop == 0) {
            for (int i = 0; i < (int)lines.size(); i++) {
                const auto& line = lines[i];
                float x0 = line.first / float(m_width / 2) * Amplitude / 2;
                float x1 = line.second / float(m_width / 2) * Amplitude / 2;
                device->SetPoint({x0, y});
                device->DrawLine({x1, y});
            }

        } else {
            for (int i = (int)lines.size() - 1; i >= 0; i--) {
                const auto& line = lines[i];
                float x0 = line.first / float(m_width / 2) * Amplitude / 2;
                float x1 = line.second / float(m_width / 2) * Amplitude / 2;
                device->SetPoint({x1, y});
                device->DrawLine({x0, y});
            }
        }
        flipflop = 1 - flipflop;
    }
}

}  // namespace AudioRender
