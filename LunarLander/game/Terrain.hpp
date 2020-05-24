#pragma once

#include <vector>

namespace LunarLander
{
struct Map {
    std::vector<int> terrain;
    std::vector<std::pair<int, int>> landingPlaces;
};

Map generateTerrain(int level, int w);

}  // namespace LunarLander
