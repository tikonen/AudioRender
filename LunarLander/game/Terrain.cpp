
#define _USE_MATH_DEFINES
#include <math.h>

#include <vector>

#include "Terrain.hpp"

namespace LunarLander
{
unsigned int Z = 0;

void randomSeed(unsigned int seed) { Z = seed; }

float random()
{
    const unsigned long long M = 4294967296;
    // a - 1 should be divisible by m's prime factors
    const unsigned long A = 1664525;
    // c and m should be co-prime
    const unsigned long C = 1;

    Z = (A * Z + C) % M;
    return Z / (float)M;
}

#if 1
float interpolate(float pa, float pb, float t)
{
    float ft = t * (float)M_PI;
    float f = (1 - cosf(ft)) * 0.5f;
    return pa * (1 - f) + pb * f;
}
#else
float interpolate(float pa, float pb, float t) { return pa * (1 - t) + pb * t; }
#endif

std::vector<int> perlin(const int wl, int const amplitude, int w)
{
    std::vector<int> arr(w);

    float a = random();
    float b = random();

    for (int x = 0; x < w; x++) {
        int y;
        if (x % wl == 0) {
            a = b;
            b = random();
            y = std::lroundf(a * amplitude);
        } else {
            y = std::lroundf(interpolate(a, b, (x % wl) / (float)wl) * amplitude);
        }
        arr[x] = y;
    }

    return arr;
}

std::vector<int> perlinOctaves(unsigned int seed, int amplitude, int wl, int octaves, int w)
{
    std::vector<int> arr(w);
    memset(&arr[0], 0, arr.size());
    const int div = 2;
    randomSeed(seed);
    for (int i = 0; i < octaves; i++) {
        auto r = perlin(wl, amplitude, w);
        for (size_t j = 0; j < r.size(); j++) arr[j] += r[j];
        amplitude /= div;
        wl /= div;
    }
    return arr;
}

std::vector<std::pair<int, int>> buildLandingPlaces(std::vector<int>& terrain)
{
    const int numLandingPlaces = 3;
    const int landingWidth = 10;
    std::vector<std::pair<int, int>> places;

    int size = (int)terrain.size();
    for (int startidx = size / numLandingPlaces - landingWidth / 2; startidx < size - landingWidth / 2; startidx += size / numLandingPlaces) {
        for (int i = startidx - 1 - landingWidth; i >= 5; i--) {
            int b = terrain[i + landingWidth];
            int a = terrain[i];
            if (std::abs(b - a) <= 1) {
                // found suitable place
                for (int j = i; j <= i + landingWidth; j++) {
                    terrain[j] = a;
                }
                terrain[i - 1]--;
                terrain[i + landingWidth + 1]--;

                places.emplace_back(i, i + landingWidth);
                break;
            }
        }
    }
    // get rid of overlapping
    for (size_t i = places.size() - 1; i > 0; i--) {
        if (places[i - 1].second >= places[i].first) {
            places.erase(places.begin() + i);
        }
    }

    return places;
}

Map generateTerrain(int level, int width)
{
    int seed = level + 1;
    int amplitude = 128;
    int waveLength = 64;
    int octaves = 3;

    auto terrain = perlinOctaves(seed, amplitude, waveLength, octaves, width);
    auto places = buildLandingPlaces(terrain);
    return {terrain, places};
}

}  // namespace LunarLander
