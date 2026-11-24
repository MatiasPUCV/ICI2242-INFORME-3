#pragma once

#include <vector>

#include "Types.hpp"

struct SearchParams {
    int maxIterations;
    int stallLimit;
    int descentSteps;
    int tenure;
    int neighborSample;
    int restarts;
    unsigned long long seed;
};

struct SearchResult {
    Solution best;
    std::vector<int> restartPallets;
    std::vector<double> restartMillis;
    std::vector<unsigned> restartViolations;
};

SearchResult RunVns(const Solution& initial, const std::vector<Box>& boxes,
                    const Container& container, const SearchParams& params);
SearchResult RunTabu(const Solution& initial, const std::vector<Box>& boxes,
                     const Container& container, const SearchParams& params);
