#include "Packing.hpp"

#include <algorithm>
#include <numeric>

static long long Area(const FreeSpace& space) {
    return static_cast<long long>(space.width) * space.depth;
}

static Score ScoreInSpace(int kind, int boxWidth, int boxDepth, const FreeSpace& space,
                          bool rotated) {
    long long waste = Area(space) - static_cast<long long>(boxWidth) * boxDepth;
    int leftover = std::min(space.width - boxWidth, space.depth - boxDepth);
    return Score{kind, waste, leftover, rotated ? 1 : 0};
}

static void SplitSpace(std::vector<FreeSpace>& spaces, const FreeSpace& space, int boxWidth,
                       int boxDepth) {
    int rightWidth = space.width - boxWidth;
    int topDepth = space.depth - boxDepth;

    FreeSpace rightFull{space.x + boxWidth, space.y, rightWidth, space.depth};
    FreeSpace topNarrow{space.x, space.y + boxDepth, boxWidth, topDepth};
    FreeSpace rightNarrow{space.x + boxWidth, space.y, rightWidth, boxDepth};
    FreeSpace topFull{space.x, space.y + boxDepth, space.width, topDepth};

    long long largestFirst = std::max(Area(rightFull), Area(topNarrow));
    long long largestSecond = std::max(Area(rightNarrow), Area(topFull));

    const FreeSpace& right = largestFirst >= largestSecond ? rightFull : rightNarrow;
    const FreeSpace& top = largestFirst >= largestSecond ? topNarrow : topFull;

    if (right.width > 0 && right.depth > 0) {
        spaces.push_back(right);
    }
    if (top.width > 0 && top.depth > 0) {
        spaces.push_back(top);
    }
}

static void PlaceInSpace(Layer& layer, int spaceIndex, const Box& box, bool rotated) {
    const FreeSpace space = layer.freeSpaces[spaceIndex];
    int placedWidth = rotated ? box.depth : box.width;
    int placedDepth = rotated ? box.width : box.depth;
    layer.placements.push_back(
        Placement{box.id, space.x, space.y, placedWidth, placedDepth, rotated});
    layer.freeSpaces.erase(layer.freeSpaces.begin() + spaceIndex);
    SplitSpace(layer.freeSpaces, space, placedWidth, placedDepth);
}

static void RebuildFreeSpaces(Layer& layer, const Container& container) {
    std::vector<int> xs{0, container.width};
    std::vector<int> ys{0, container.depth};
    for (const Placement& placement : layer.placements) {
        xs.push_back(placement.x);
        xs.push_back(placement.x + placement.width);
        ys.push_back(placement.y);
        ys.push_back(placement.y + placement.depth);
    }
    std::sort(xs.begin(), xs.end());
    xs.erase(std::unique(xs.begin(), xs.end()), xs.end());
    std::sort(ys.begin(), ys.end());
    ys.erase(std::unique(ys.begin(), ys.end()), ys.end());

    const int columns = static_cast<int>(xs.size()) - 1;
    const int rows = static_cast<int>(ys.size()) - 1;
    std::vector<char> taken(static_cast<size_t>(columns) * rows, 0);

    for (const Placement& placement : layer.placements) {
        int firstColumn = static_cast<int>(std::lower_bound(xs.begin(), xs.end(), placement.x) -
                                           xs.begin());
        int lastColumn = static_cast<int>(
            std::lower_bound(xs.begin(), xs.end(), placement.x + placement.width) - xs.begin());
        int firstRow = static_cast<int>(std::lower_bound(ys.begin(), ys.end(), placement.y) -
                                        ys.begin());
        int lastRow = static_cast<int>(
            std::lower_bound(ys.begin(), ys.end(), placement.y + placement.depth) - ys.begin());
        for (int row = firstRow; row < lastRow; ++row) {
            for (int column = firstColumn; column < lastColumn; ++column) {
                taken[static_cast<size_t>(row) * columns + column] = 1;
            }
        }
    }

    layer.freeSpaces.clear();
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            if (taken[static_cast<size_t>(row) * columns + column]) {
                continue;
            }
            int endColumn = column;
            while (endColumn < columns && !taken[static_cast<size_t>(row) * columns + endColumn]) {
                ++endColumn;
            }
            int endRow = row + 1;
            while (endRow < rows) {
                bool rowFree = true;
                for (int k = column; k < endColumn; ++k) {
                    if (taken[static_cast<size_t>(endRow) * columns + k]) {
                        rowFree = false;
                        break;
                    }
                }
                if (!rowFree) {
                    break;
                }
                ++endRow;
            }
            for (int r = row; r < endRow; ++r) {
                for (int c = column; c < endColumn; ++c) {
                    taken[static_cast<size_t>(r) * columns + c] = 1;
                }
            }
            layer.freeSpaces.push_back(FreeSpace{xs[column], ys[row], xs[endColumn] - xs[column],
                                                 ys[endRow] - ys[row]});
        }
    }
}

bool FitsAlone(const Box& box, const Container& container) {
    bool upright = box.width <= container.width && box.depth <= container.depth;
    bool turned = box.depth <= container.width && box.width <= container.depth;
    return (upright || turned) && box.height <= container.height;
}

bool FitsAll(const std::vector<Box>& boxes, const Container& container) {
    return std::all_of(boxes.begin(), boxes.end(),
                       [&](const Box& box) { return FitsAlone(box, container); });
}

bool BoxPrecedes(const Box& first, const Box& second) {
    if (first.height != second.height) {
        return first.height > second.height;
    }
    long long firstArea = static_cast<long long>(first.width) * first.depth;
    long long secondArea = static_cast<long long>(second.width) * second.depth;
    if (firstArea != secondArea) {
        return firstArea > secondArea;
    }
    return first.id < second.id;
}

std::vector<int> SortedBoxOrder(const std::vector<Box>& boxes) {
    std::vector<int> order(boxes.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(),
              [&](int first, int second) { return BoxPrecedes(boxes[first], boxes[second]); });
    return order;
}

void EnumeratePlacements(const Solution& solution, const Box& box, const Container& container,
                         const SearchFilter& filter, std::vector<Candidate>& candidates) {
    candidates.clear();
    const int palletCount = static_cast<int>(solution.pallets.size());
    const bool square = box.width == box.depth;
    const FreeSpace fullSpace{0, 0, container.width, container.depth};

    for (int p = 0; p < palletCount; ++p) {
        if (p == filter.blockedPallet) {
            continue;
        }
        if (p == filter.skipPallet && filter.skipLayer == NO_INDEX) {
            continue;
        }
        const PalletState& pallet = solution.pallets[p];
        const int layerCount = static_cast<int>(pallet.layers.size());

        for (int l = 0; l < layerCount; ++l) {
            if (p == filter.skipPallet && l == filter.skipLayer) {
                continue;
            }
            const Layer& layer = pallet.layers[l];
            if (layer.height != box.height) {
                continue;
            }
            const int spaceCount = static_cast<int>(layer.freeSpaces.size());
            for (int s = 0; s < spaceCount; ++s) {
                const FreeSpace& space = layer.freeSpaces[s];
                for (int r = 0; r < 2; ++r) {
                    if (r == 1 && square) {
                        break;
                    }
                    int boxWidth = r == 1 ? box.depth : box.width;
                    int boxDepth = r == 1 ? box.width : box.depth;
                    if (boxWidth <= space.width && boxDepth <= space.depth) {
                        candidates.push_back(
                            Candidate{PlacementKind::EXISTING_LAYER, p, l, s, r == 1,
                                      ScoreInSpace(0, boxWidth, boxDepth, space, r == 1)});
                    }
                }
            }
        }

        if (pallet.usedHeight + box.height <= container.height) {
            long long remaining = container.height - pallet.usedHeight - box.height;
            for (int r = 0; r < 2; ++r) {
                if (r == 1 && square) {
                    break;
                }
                int boxWidth = r == 1 ? box.depth : box.width;
                int boxDepth = r == 1 ? box.width : box.depth;
                if (boxWidth <= container.width && boxDepth <= container.depth) {
                    Score score = ScoreInSpace(1, boxWidth, boxDepth, fullSpace, r == 1);
                    score.primary = remaining;
                    candidates.push_back(
                        Candidate{PlacementKind::NEW_LAYER, p, NO_INDEX, 0, r == 1, score});
                }
            }
        }
    }

    if (filter.allowNewPallet) {
        for (int r = 0; r < 2; ++r) {
            if (r == 1 && square) {
                break;
            }
            int boxWidth = r == 1 ? box.depth : box.width;
            int boxDepth = r == 1 ? box.width : box.depth;
            if (boxWidth <= container.width && boxDepth <= container.depth) {
                Score score = ScoreInSpace(2, boxWidth, boxDepth, fullSpace, r == 1);
                score.primary = 0;
                candidates.push_back(
                    Candidate{PlacementKind::NEW_PALLET, palletCount, NO_INDEX, 0, r == 1, score});
            }
        }
    }
}

std::optional<Candidate> FindBestPlacement(const Solution& solution, const Box& box,
                                           const Container& container,
                                           const SearchFilter& filter) {
    static thread_local std::vector<Candidate> buffer;
    EnumeratePlacements(solution, box, container, filter, buffer);
    if (buffer.empty()) {
        return std::nullopt;
    }
    auto best = std::min_element(buffer.begin(), buffer.end(),
                                 [](const Candidate& first, const Candidate& second) {
                                     return first.score < second.score;
                                 });
    return *best;
}

std::optional<Candidate> FindFirstPlacement(const Solution& solution, const Box& box,
                                            const Container& container) {
    static thread_local std::vector<Candidate> buffer;
    EnumeratePlacements(solution, box, container, UNRESTRICTED, buffer);
    if (buffer.empty()) {
        return std::nullopt;
    }
    auto first = std::min_element(buffer.begin(), buffer.end(),
                                  [](const Candidate& left, const Candidate& right) {
                                      return left.score.kind < right.score.kind;
                                  });
    return *first;
}

void ApplyPlacement(Solution& solution, const Box& box, const Container& container,
                    const Candidate& candidate) {
    int palletIndex = candidate.palletIndex;
    if (candidate.kind == PlacementKind::NEW_PALLET) {
        solution.pallets.push_back(PalletState{solution.nextPalletId++, 0, 0, {}});
        palletIndex = static_cast<int>(solution.pallets.size()) - 1;
    }
    PalletState& pallet = solution.pallets[palletIndex];

    int layerIndex = candidate.layerIndex;
    int spaceIndex = candidate.spaceIndex;
    if (candidate.kind != PlacementKind::EXISTING_LAYER) {
        pallet.layers.push_back(
            Layer{box.height, {FreeSpace{0, 0, container.width, container.depth}}, {}});
        pallet.usedHeight += box.height;
        layerIndex = static_cast<int>(pallet.layers.size()) - 1;
        spaceIndex = 0;
    }

    PlaceInSpace(pallet.layers[layerIndex], spaceIndex, box, candidate.rotated);
    pallet.load += BoxVolume(box);
}

void RemovePlacement(Solution& solution, const Box& box, const Container& container,
                     int palletIndex, int layerIndex, int placementIndex) {
    PalletState& pallet = solution.pallets[palletIndex];
    Layer& layer = pallet.layers[layerIndex];
    layer.placements.erase(layer.placements.begin() + placementIndex);
    pallet.load -= BoxVolume(box);

    if (layer.placements.empty()) {
        pallet.usedHeight -= layer.height;
        pallet.layers.erase(pallet.layers.begin() + layerIndex);
        if (pallet.layers.empty()) {
            solution.pallets.erase(solution.pallets.begin() + palletIndex);
        }
    } else {
        RebuildFreeSpaces(layer, container);
    }
}

bool LocateBox(const Solution& solution, int boxId, int& palletIndex, int& layerIndex,
               int& placementIndex) {
    for (int p = 0; p < static_cast<int>(solution.pallets.size()); ++p) {
        const PalletState& pallet = solution.pallets[p];
        for (int l = 0; l < static_cast<int>(pallet.layers.size()); ++l) {
            const Layer& layer = pallet.layers[l];
            for (int i = 0; i < static_cast<int>(layer.placements.size()); ++i) {
                if (layer.placements[i].boxId == boxId) {
                    palletIndex = p;
                    layerIndex = l;
                    placementIndex = i;
                    return true;
                }
            }
        }
    }
    return false;
}

Cost ComputeCost(const Solution& solution) {
    long long sum = 0;
    for (const PalletState& pallet : solution.pallets) {
        sum += pallet.load * pallet.load;
    }
    return Cost{static_cast<int>(solution.pallets.size()), -sum};
}

Cost CostAfterMove(const Solution& solution, int fromPallet, int toPallet, long long volume) {
    int count = 0;
    long long sum = 0;
    for (int p = 0; p < static_cast<int>(solution.pallets.size()); ++p) {
        long long load = solution.pallets[p].load;
        if (p == fromPallet) {
            load -= volume;
        }
        if (p == toPallet) {
            load += volume;
        }
        if (load > 0) {
            ++count;
            sum += load * load;
        }
    }
    return Cost{count, -sum};
}

std::vector<int> PalletsByLoad(const Solution& solution) {
    std::vector<int> order(solution.pallets.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int first, int second) {
        if (solution.pallets[first].load != solution.pallets[second].load) {
            return solution.pallets[first].load < solution.pallets[second].load;
        }
        return first < second;
    });
    return order;
}

std::vector<int> BoxIdsOfPallet(const PalletState& pallet) {
    std::vector<int> ids;
    for (const Layer& layer : pallet.layers) {
        for (const Placement& placement : layer.placements) {
            ids.push_back(placement.boxId);
        }
    }
    return ids;
}
