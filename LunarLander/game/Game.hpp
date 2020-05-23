#pragma once

#include <atomic>

#include <DrawDevice.hpp>

namespace LunarLander
{
class Game
{
public:
    void mainLoop(std::atomic_bool& running, AudioRender::IDrawDevice* device);
};

}  // namespace LunarLander
