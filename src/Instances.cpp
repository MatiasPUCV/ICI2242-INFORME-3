#include "Instances.hpp"

#include <random>

static int RandomInt(std::mt19937_64& rng, int low, int high) {
    return std::uniform_int_distribution<int>(low, high)(rng);
}

static int PickFrom(std::mt19937_64& rng, const std::vector<int>& values) {
    return values[RandomInt(rng, 0, static_cast<int>(values.size()) - 1)];
}

unsigned long long MixSeed(unsigned long long first, unsigned long long second,
                           unsigned long long third) {
    unsigned long long value = first * 0x9E3779B97F4A7C15ULL;
    value ^= (second + 0x7F4A7C15ULL) * 0xBF58476D1CE4E5B9ULL;
    value ^= (third + 0x1234567ULL) * 0x94D049BB133111EBULL;
    value ^= value >> 31;
    return value;
}

Instance GenerateScenarioInstance(Scenario scenario, int boxCount, unsigned long long seed) {
    std::mt19937_64 rng(seed);
    Instance instance{{}, SCENARIO_CONTAINER};
    const std::vector<int> homogeneousHeights{30, 40};
    const std::vector<int> heterogeneousHeights{10, 20, 30, 40, 60};
    const std::vector<int> fragmentedHeights{20, 30, 50};

    for (int id = 0; id < boxCount; ++id) {
        Box box{id, 0, 0, 0};
        if (scenario == Scenario::HOMOGENEOUS) {
            box.width = RandomInt(rng, 36, 44);
            box.depth = RandomInt(rng, 26, 34);
            box.height = PickFrom(rng, homogeneousHeights);
        } else if (scenario == Scenario::HETEROGENEOUS) {
            box.width = RandomInt(rng, 10, 60);
            box.depth = RandomInt(rng, 10, 50);
            box.height = PickFrom(rng, heterogeneousHeights);
        } else {
            if (RandomInt(rng, 0, 9) < 6) {
                box.width = RandomInt(rng, 61, 70);
                box.depth = RandomInt(rng, 41, 50);
            } else {
                box.width = RandomInt(rng, 10, 30);
                box.depth = RandomInt(rng, 10, 30);
            }
            box.height = PickFrom(rng, fragmentedHeights);
        }
        instance.boxes.push_back(box);
    }
    return instance;
}

Instance GenerateValidationInstance(ValidationCase validationCase, unsigned long long seed) {
    std::mt19937_64 rng(seed);
    Instance instance{{}, Container{10, 8, 6}};
    const int boxCount = RandomInt(rng, 8, 14);

    for (int id = 0; id < boxCount; ++id) {
        Box box{id, 0, 0, 0};
        switch (validationCase) {
        case ValidationCase::SIMPLE:
            box.width = RandomInt(rng, 4, 5);
            box.depth = RandomInt(rng, 3, 4);
            box.height = RandomInt(rng, 2, 3);
            break;
        case ValidationCase::ROTATION:
            box.width = RandomInt(rng, 2, 4);
            box.depth = RandomInt(rng, 9, 10);
            box.height = RandomInt(rng, 2, 3);
            break;
        case ValidationCase::MULTILAYER:
            box.width = RandomInt(rng, 9, 10);
            box.depth = RandomInt(rng, 7, 8);
            box.height = RandomInt(rng, 1, 3);
            break;
        case ValidationCase::FRAGMENTATION:
            if (RandomInt(rng, 0, 1) == 0) {
                box.width = RandomInt(rng, 6, 7);
                box.depth = RandomInt(rng, 5, 6);
            } else {
                box.width = RandomInt(rng, 1, 3);
                box.depth = RandomInt(rng, 1, 3);
            }
            box.height = RandomInt(rng, 2, 3);
            break;
        case ValidationCase::INFEASIBLE:
            box.width = RandomInt(rng, 3, 6);
            box.depth = RandomInt(rng, 3, 6);
            box.height = RandomInt(rng, 1, 3);
            break;
        }
        instance.boxes.push_back(box);
    }

    if (validationCase == ValidationCase::INFEASIBLE) {
        Box& culprit = instance.boxes[RandomInt(rng, 0, boxCount - 1)];
        if (RandomInt(rng, 0, 1) == 0) {
            culprit.height = RandomInt(rng, 7, 9);
        } else {
            culprit.width = RandomInt(rng, 11, 12);
            culprit.depth = RandomInt(rng, 9, 10);
        }
    }
    return instance;
}

Instance ReportExampleInstance() {
    Instance instance{{}, Container{10, 8, 6}};
    const int data[14][3] = {{6, 5, 3}, {4, 5, 3}, {5, 3, 3}, {3, 5, 3}, {10, 4, 3},
                             {5, 4, 3}, {4, 5, 3}, {8, 4, 4}, {4, 4, 4}, {3, 4, 4},
                             {2, 8, 4}, {10, 3, 2}, {6, 3, 2}, {4, 3, 2}};
    for (int id = 0; id < 14; ++id) {
        instance.boxes.push_back(Box{id, data[id][0], data[id][1], data[id][2]});
    }
    return instance;
}

std::string ScenarioCode(Scenario scenario) {
    switch (scenario) {
    case Scenario::HOMOGENEOUS:
        return "A";
    case Scenario::HETEROGENEOUS:
        return "B";
    case Scenario::FRAGMENTED:
        return "C";
    }
    return "";
}

std::string ValidationCaseCode(ValidationCase validationCase) {
    switch (validationCase) {
    case ValidationCase::SIMPLE:
        return "SIMPLE";
    case ValidationCase::ROTATION:
        return "ROTATION";
    case ValidationCase::MULTILAYER:
        return "MULTILAYER";
    case ValidationCase::FRAGMENTATION:
        return "FRAGMENTATION";
    case ValidationCase::INFEASIBLE:
        return "INFEASIBLE";
    }
    return "";
}
