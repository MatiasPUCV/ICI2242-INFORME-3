#pragma once

#include <compare>
#include <vector>

constexpr int NO_INDEX = -1;

struct Box
{
    int id;
    int width;
    int depth;
    int height;
};

struct Container
{
    int width;
    int depth;
    int height;
};

struct FreeSpace
{
    int x;
    int y;
    int width;
    int depth;
};

struct Placement
{
    int boxId;
    int x;
    int y;
    int width;
    int depth;
    bool rotated;
};

struct Layer
{
    int height;
    std::vector<FreeSpace> freeSpaces;
    std::vector<Placement> placements;
};

struct PalletState
{
    int id;
    int usedHeight;
    long long load;
    std::vector<Layer> layers;
};

struct Solution
{
    std::vector<PalletState> pallets;
    int nextPalletId = 0;
};

struct Cost
{
    int pallets;
    long long concentration;
    auto operator<=>(const Cost&) const = default;
};

inline long long BoxVolume(const Box& box)
{
    return static_cast<long long>(box.width) * box.depth * box.height;
}

inline long long ContainerVolume(const Container& container)
{
    return static_cast<long long>(container.width) * container.depth * container.height;
}
