#pragma once

#include <optional>
#include <vector>

#include "Types.hpp"

enum class PlacementKind { EXISTING_LAYER, NEW_LAYER, NEW_PALLET };

struct Score {
    int kind;
    long long primary;
    int secondary;
    int rotated;
    auto operator<=>(const Score&) const = default;
};

struct Candidate {
    PlacementKind kind;
    int palletIndex;
    int layerIndex;
    int spaceIndex;
    bool rotated;
    Score score;
};

struct SearchFilter {
    int skipPallet;
    int skipLayer;
    int blockedPallet;
    bool allowNewPallet;
};

constexpr SearchFilter UNRESTRICTED = {NO_INDEX, NO_INDEX, NO_INDEX, true};
constexpr SearchFilter EXISTING_ONLY = {NO_INDEX, NO_INDEX, NO_INDEX, false};

bool FitsAlone(const Box& box, const Container& container);
bool FitsAll(const std::vector<Box>& boxes, const Container& container);
bool BoxPrecedes(const Box& first, const Box& second);
std::vector<int> SortedBoxOrder(const std::vector<Box>& boxes);

void EnumeratePlacements(const Solution& solution, const Box& box, const Container& container,
                         const SearchFilter& filter, std::vector<Candidate>& candidates);
std::optional<Candidate> FindBestPlacement(const Solution& solution, const Box& box,
                                           const Container& container, const SearchFilter& filter);
std::optional<Candidate> FindFirstPlacement(const Solution& solution, const Box& box,
                                            const Container& container);

void ApplyPlacement(Solution& solution, const Box& box, const Container& container,
                    const Candidate& candidate);
void RemovePlacement(Solution& solution, const Box& box, const Container& container,
                     int palletIndex, int layerIndex, int placementIndex);
bool LocateBox(const Solution& solution, int boxId, int& palletIndex, int& layerIndex,
               int& placementIndex);

Cost ComputeCost(const Solution& solution);
Cost CostAfterMove(const Solution& solution, int fromPallet, int toPallet, long long volume);
std::vector<int> PalletsByLoad(const Solution& solution);
std::vector<int> BoxIdsOfPallet(const PalletState& pallet);
