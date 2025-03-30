#pragma once

namespace AudioRender
{
class RasterImage
{
public:
    bool loadImage(const char* filename);
    bool loadImage(const uint8_t* buffer, int len);
    void drawImage(IDrawDevice* device);

private:
    bool processImageData(const uint8_t* buffer, int width, int height);

    std::vector<std::vector<std::pair<int, int>>> m_scanLines;
    int m_width;
    int m_height;
};
}  // namespace AudioRender
