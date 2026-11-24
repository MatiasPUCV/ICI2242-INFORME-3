#pragma once

#include <string>
#include <vector>

#include "Types.hpp"

enum class Scenario { HOMOGENEOUS, HETEROGENEOUS, FRAGMENTED };

enum class ValidationCase { SIMPLE, ROTATION, MULTILAYER, FRAGMENTATION, INFEASIBLE };

struct Instance {
    std::vector<Box> boxes;
    Container container;
};

constexpr Container SCENARIO_CONTAINER = {120, 80, 180};

Instance GenerateScenarioInstance(Scenario scenario, int boxCount, unsigned long long seed);
Instance GenerateValidationInstance(ValidationCase validationCase, unsigned long long seed);
Instance ReportExampleInstance();
std::string ScenarioCode(Scenario scenario);
std::string ValidationCaseCode(ValidationCase validationCase);
unsigned long long MixSeed(unsigned long long first, unsigned long long second,
                           unsigned long long third);
