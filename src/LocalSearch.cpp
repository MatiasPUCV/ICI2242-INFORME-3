#include "LocalSearch.hpp"

#include <algorithm>
#include <chrono>
#include <optional>
#include <random>
#include <unordered_map>

#include "Packing.hpp"
#include "Validator.hpp"

constexpr int ELIMINATION_TRIES = 2;
constexpr int DESCENT_SOURCE_PALLETS = 3;
constexpr int MAX_SHAKE_MOVES = 3;
constexpr int SHAKE_ATTEMPTS = 12;
constexpr int VNS_NEIGHBORHOODS = 2;
constexpr int TABU_TENURE_JITTER = 3;
constexpr long long TABU_KEY_STRIDE = 1000003;
constexpr unsigned long long RESTART_SEED_STRIDE = 7919;

using SearchFunction = Solution (*)(const Solution&, const std::vector<Box>&, const Container&,
                                    const SearchParams&, std::mt19937_64&);

static int RandomIndex(std::mt19937_64& rng, int bound) {
    return std::uniform_int_distribution<int>(0, bound - 1)(rng);
}

static void ApplyRelocation(Solution& solution, const std::vector<Box>& boxes,
                            const Container& container, int boxId, const Candidate& candidate) {
    int palletIndex = 0;
    int layerIndex = 0;
    int placementIndex = 0;
    LocateBox(solution, boxId, palletIndex, layerIndex, placementIndex);
    ApplyPlacement(solution, boxes[boxId], container, candidate);
    RemovePlacement(solution, boxes[boxId], container, palletIndex, layerIndex, placementIndex);
}

static std::optional<Candidate> FindRelocation(const Solution& solution, const Box& box,
                                               const Container& container, int sourcePallet,
                                               int blockedPallet) {
    SearchFilter filter{sourcePallet, NO_INDEX, blockedPallet, false};
    return FindBestPlacement(solution, box, container, filter);
}

static std::optional<Solution> EliminatePallet(const Solution& solution,
                                               const std::vector<Box>& boxes,
                                               const Container& container, int palletIndex) {
    long long totalLoad = 0;
    for (const PalletState& pallet : solution.pallets) {
        totalLoad += pallet.load;
    }
    long long remainingCapacity =
        static_cast<long long>(solution.pallets.size() - 1) * ContainerVolume(container);
    if (remainingCapacity < totalLoad) {
        return std::nullopt;
    }

    Solution trial = solution;
    std::vector<int> ids = BoxIdsOfPallet(trial.pallets[palletIndex]);
    trial.pallets.erase(trial.pallets.begin() + palletIndex);
    std::sort(ids.begin(), ids.end(),
              [&](int first, int second) { return BoxPrecedes(boxes[first], boxes[second]); });

    for (int boxId : ids) {
        std::optional<Candidate> candidate =
            FindBestPlacement(trial, boxes[boxId], container, EXISTING_ONLY);
        if (!candidate) {
            return std::nullopt;
        }
        ApplyPlacement(trial, boxes[boxId], container, *candidate);
    }
    return trial;
}

static void LocalDescent(Solution& solution, const std::vector<Box>& boxes,
                         const Container& container, int maxSteps) {
    for (int step = 0; step < maxSteps; ++step) {
        if (solution.pallets.size() < 2) {
            return;
        }
        const Cost current = ComputeCost(solution);
        const std::vector<int> order = PalletsByLoad(solution);
        const int palletCount = static_cast<int>(order.size());
        bool improved = false;

        for (int t = 0; t < std::min(ELIMINATION_TRIES, palletCount) && !improved; ++t) {
            std::optional<Solution> trial = EliminatePallet(solution, boxes, container, order[t]);
            if (trial) {
                solution = std::move(*trial);
                improved = true;
            }
        }
        if (improved) {
            continue;
        }

        const int sources = std::min(DESCENT_SOURCE_PALLETS, palletCount - 1);
        for (int s = 0; s < sources && !improved; ++s) {
            int sourceIndex = order[s];
            for (int boxId : BoxIdsOfPallet(solution.pallets[sourceIndex])) {
                std::optional<Candidate> candidate =
                    FindRelocation(solution, boxes[boxId], container, sourceIndex, NO_INDEX);
                if (!candidate) {
                    continue;
                }
                Cost after = CostAfterMove(solution, sourceIndex, candidate->palletIndex,
                                           BoxVolume(boxes[boxId]));
                if (after < current) {
                    ApplyRelocation(solution, boxes, container, boxId, *candidate);
                    improved = true;
                    break;
                }
            }
        }
        if (!improved) {
            return;
        }
    }
}

static std::optional<Solution> ShakeRelocate(const Solution& current,
                                             const std::vector<Box>& boxes,
                                             const Container& container, std::mt19937_64& rng) {
    Solution candidate = current;
    const int moves = 1 + RandomIndex(rng, MAX_SHAKE_MOVES);
    int applied = 0;
    std::vector<Candidate> options;

    for (int attempt = 0; attempt < SHAKE_ATTEMPTS && applied < moves; ++attempt) {
        int boxId = RandomIndex(rng, static_cast<int>(boxes.size()));
        int palletIndex = 0;
        int layerIndex = 0;
        int placementIndex = 0;
        LocateBox(candidate, boxId, palletIndex, layerIndex, placementIndex);
        SearchFilter filter{palletIndex, layerIndex, NO_INDEX, false};
        EnumeratePlacements(candidate, boxes[boxId], container, filter, options);
        if (options.empty()) {
            continue;
        }
        Candidate chosen = options[RandomIndex(rng, static_cast<int>(options.size()))];
        ApplyRelocation(candidate, boxes, container, boxId, chosen);
        ++applied;
    }

    if (applied == 0) {
        return std::nullopt;
    }
    return candidate;
}

static std::optional<Solution> ShakeEliminate(const Solution& current,
                                              const std::vector<Box>& boxes,
                                              const Container& container, std::mt19937_64& rng) {
    const int palletCount = static_cast<int>(current.pallets.size());
    if (palletCount < 2) {
        return std::nullopt;
    }
    const std::vector<int> order = PalletsByLoad(current);
    const int pool = std::min(palletCount, std::max(2, palletCount / 3));
    return EliminatePallet(current, boxes, container, order[RandomIndex(rng, pool)]);
}

static Solution VnsOnce(const Solution& initial, const std::vector<Box>& boxes,
                        const Container& container, const SearchParams& params,
                        std::mt19937_64& rng) {
    Solution best = initial;
    Solution current = initial;
    Cost bestCost = ComputeCost(best);
    Cost currentCost = bestCost;
    int neighborhood = 1;
    int stall = 0;

    for (int iteration = 0; iteration < params.maxIterations && stall < params.stallLimit;
         ++iteration) {
        std::optional<Solution> candidate = neighborhood == 1
                                                ? ShakeRelocate(current, boxes, container, rng)
                                                : ShakeEliminate(current, boxes, container, rng);
        bool accepted = false;
        bool foundBest = false;
        if (candidate) {
            LocalDescent(*candidate, boxes, container, params.descentSteps);
            Cost candidateCost = ComputeCost(*candidate);
            if (candidateCost < currentCost) {
                current = std::move(*candidate);
                currentCost = candidateCost;
                neighborhood = 1;
                accepted = true;
                if (currentCost < bestCost) {
                    best = current;
                    bestCost = currentCost;
                    foundBest = true;
                }
            }
        }
        if (!accepted) {
            neighborhood = neighborhood % VNS_NEIGHBORHOODS + 1;
        }
        stall = foundBest ? 0 : stall + 1;
    }
    return best;
}

static bool IsTabu(const std::unordered_map<long long, int>& tabu, int boxId, int palletId,
                   int iteration) {
    auto found = tabu.find(static_cast<long long>(boxId) * TABU_KEY_STRIDE + palletId);
    return found != tabu.end() && found->second > iteration;
}

static Solution TabuOnce(const Solution& initial, const std::vector<Box>& boxes,
                         const Container& container, const SearchParams& params,
                         std::mt19937_64& rng) {
    Solution best = initial;
    Solution current = initial;
    Cost bestCost = ComputeCost(best);
    std::unordered_map<long long, int> tabu;
    int stall = 0;

    for (int iteration = 0; iteration < params.maxIterations && stall < params.stallLimit;
         ++iteration) {
        const int palletCount = static_cast<int>(current.pallets.size());
        if (palletCount < 2) {
            break;
        }
        const std::vector<int> order = PalletsByLoad(current);

        bool haveMove = false;
        Cost moveCost{0, 0};
        int moveBox = NO_INDEX;
        int moveFromId = NO_INDEX;
        Candidate moveCandidate{};
        std::optional<Solution> moveSolution;

        for (int t = 0; t < std::min(ELIMINATION_TRIES, palletCount); ++t) {
            std::optional<Solution> trial = EliminatePallet(current, boxes, container, order[t]);
            if (!trial) {
                continue;
            }
            Cost trialCost = ComputeCost(*trial);
            if (!haveMove || trialCost < moveCost) {
                haveMove = true;
                moveCost = trialCost;
                moveSolution = std::move(trial);
            }
        }

        for (int sample = 0; sample < params.neighborSample; ++sample) {
            int boxId = 0;
            if (RandomIndex(rng, 2) == 0) {
                int pool = std::min(2, palletCount);
                std::vector<int> ids =
                    BoxIdsOfPallet(current.pallets[order[RandomIndex(rng, pool)]]);
                boxId = ids[RandomIndex(rng, static_cast<int>(ids.size()))];
            } else {
                boxId = RandomIndex(rng, static_cast<int>(boxes.size()));
            }

            int palletIndex = 0;
            int layerIndex = 0;
            int placementIndex = 0;
            LocateBox(current, boxId, palletIndex, layerIndex, placementIndex);
            const Box& box = boxes[boxId];
            long long volume = BoxVolume(box);

            std::optional<Candidate> candidate =
                FindRelocation(current, box, container, palletIndex, NO_INDEX);
            if (!candidate) {
                continue;
            }
            Cost after = CostAfterMove(current, palletIndex, candidate->palletIndex, volume);
            bool forbidden =
                IsTabu(tabu, boxId, current.pallets[candidate->palletIndex].id, iteration);
            if (forbidden && !(after < bestCost)) {
                candidate =
                    FindRelocation(current, box, container, palletIndex, candidate->palletIndex);
                if (!candidate) {
                    continue;
                }
                after = CostAfterMove(current, palletIndex, candidate->palletIndex, volume);
                forbidden =
                    IsTabu(tabu, boxId, current.pallets[candidate->palletIndex].id, iteration);
                if (forbidden && !(after < bestCost)) {
                    continue;
                }
            }

            if (!haveMove || after < moveCost) {
                haveMove = true;
                moveCost = after;
                moveBox = boxId;
                moveFromId = current.pallets[palletIndex].id;
                moveCandidate = *candidate;
                moveSolution.reset();
            }
        }

        if (!haveMove) {
            break;
        }

        if (moveSolution) {
            current = std::move(*moveSolution);
        } else {
            tabu[static_cast<long long>(moveBox) * TABU_KEY_STRIDE + moveFromId] =
                iteration + params.tenure + RandomIndex(rng, TABU_TENURE_JITTER);
            ApplyRelocation(current, boxes, container, moveBox, moveCandidate);
        }

        Cost currentCost = ComputeCost(current);
        if (currentCost < bestCost) {
            best = current;
            bestCost = currentCost;
            stall = 0;
        } else {
            ++stall;
        }
    }
    return best;
}

static SearchResult RunRestarts(SearchFunction search, const Solution& initial,
                                const std::vector<Box>& boxes, const Container& container,
                                const SearchParams& params) {
    SearchResult result;
    result.best = initial;
    Cost bestCost = ComputeCost(initial);

    for (int restart = 0; restart < params.restarts; ++restart) {
        std::mt19937_64 rng(params.seed + RESTART_SEED_STRIDE * (restart + 1));
        auto start = std::chrono::steady_clock::now();
        Solution solution = search(initial, boxes, container, params, rng);
        auto stop = std::chrono::steady_clock::now();

        result.restartPallets.push_back(static_cast<int>(solution.pallets.size()));
        result.restartMillis.push_back(
            std::chrono::duration<double, std::milli>(stop - start).count());
        result.restartViolations.push_back(Validate(solution, boxes, container));

        Cost cost = ComputeCost(solution);
        if (cost < bestCost) {
            bestCost = cost;
            result.best = std::move(solution);
        }
    }
    return result;
}

SearchResult RunVns(const Solution& initial, const std::vector<Box>& boxes,
                    const Container& container, const SearchParams& params) {
    return RunRestarts(VnsOnce, initial, boxes, container, params);
}

SearchResult RunTabu(const Solution& initial, const std::vector<Box>& boxes,
                     const Container& container, const SearchParams& params) {
    return RunRestarts(TabuOnce, initial, boxes, container, params);
}
