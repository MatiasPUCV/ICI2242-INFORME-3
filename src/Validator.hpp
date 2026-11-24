#pragma once

#include <vector>

#include "Types.hpp"

constexpr unsigned int VIOLATION_COVERAGE      = 1 << 0;
constexpr unsigned int VIOLATION_BOUNDS        = 1 << 1;
constexpr unsigned int VIOLATION_OVERLAP       = 1 << 2;
constexpr unsigned int VIOLATION_LAYER_HEIGHT  = 1 << 3;
constexpr unsigned int VIOLATION_PALLET_HEIGHT = 1 << 4;
constexpr unsigned int VIOLATION_ORIENTATION   = 1 << 5;

unsigned int Validate(const Solution& solution, const std::vector<Box>& boxes, const Container& container);
