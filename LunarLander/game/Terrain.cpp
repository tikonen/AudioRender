
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

std::vector<int> generateTerrain(int level, int width)
{
    int seed = level + 1;
    int amplitude = 128;
    int waveLength = 64;
    int octaves = 5;

    auto terrain = perlinOctaves(seed, amplitude, waveLength, octaves, width);
    return terrain;
}

}  // namespace LunarLander
