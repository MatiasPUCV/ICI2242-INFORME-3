#pragma once

#include <optional>
#include <vector>

#include "Types.hpp"

std::optional<Solution> FirstFit(const std::vector<Box>& boxes, const Container& container);
std::optional<Solution> BestFitDecreasing(const std::vector<Box>& boxes,
                                          const Container& container);
