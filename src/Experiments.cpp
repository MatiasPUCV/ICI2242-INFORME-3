#include "Experiments.hpp"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <optional>

#include "Greedy.hpp"
#include "Instances.hpp"
#include "Packing.hpp"
#include "Validator.hpp"

enum class Algorithm
{
    FIRST_FIT,
    BEST_FIT_DECREASING,
    VNS,
    TABU
};

struct AlgorithmRun
{
    Algorithm algorithm;
    bool produced;
    int pallets;
    unsigned violations;
    double millis;
    std::vector<int> restartPallets;
    std::vector<double> restartMillis;
    std::vector<unsigned> restartViolations;
    Solution solution;
};

static const Algorithm ALL_ALGORITHMS[] =
{
    Algorithm::FIRST_FIT,
    Algorithm::BEST_FIT_DECREASING,
    Algorithm::VNS, Algorithm::TABU
};

static std::string AlgorithmCode(Algorithm algorithm)
{
    switch (algorithm)
    {
        case Algorithm::FIRST_FIT:           return "FF";
        case Algorithm::BEST_FIT_DECREASING: return "BFD";
        case Algorithm::VNS:                 return "VNS";
        case Algorithm::TABU:                return "TABU";
    }

    return "";
}

static double ElapsedMillis(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
        .count();
}

static AlgorithmRun MakeGreedyRun(Algorithm algorithm, std::optional<Solution> solution,
                                  double millis, const Instance& instance) {
    AlgorithmRun run{algorithm, solution.has_value(), 0, 0, millis, {}, {}, {}, {}};
    if (solution) {
        run.solution = std::move(*solution);
        run.pallets = static_cast<int>(run.solution.pallets.size());
        run.violations = Validate(run.solution, instance.boxes, instance.container);
    }
    return run;
}

static AlgorithmRun MakeSearchRun(Algorithm algorithm, SearchResult result,
                                  const Instance& instance) {
    AlgorithmRun run{algorithm, true, 0, 0, 0.0, {}, {}, {}, {}};
    run.solution = std::move(result.best);
    run.pallets = static_cast<int>(run.solution.pallets.size());
    run.violations = Validate(run.solution, instance.boxes, instance.container);
    for (unsigned restartViolation : result.restartViolations) {
        run.violations |= restartViolation;
    }
    run.restartViolations = std::move(result.restartViolations);
    run.restartPallets = std::move(result.restartPallets);
    run.restartMillis = std::move(result.restartMillis);
    run.millis = std::accumulate(run.restartMillis.begin(), run.restartMillis.end(), 0.0);
    return run;
}

static std::vector<AlgorithmRun> RunAllAlgorithms(const Instance& instance,
                                                  const ExperimentConfig& config,
                                                  unsigned long long seed) {
    std::vector<AlgorithmRun> runs;

    auto start = std::chrono::steady_clock::now();
    std::optional<Solution> firstFit = FirstFit(instance.boxes, instance.container);
    runs.push_back(
        MakeGreedyRun(Algorithm::FIRST_FIT, std::move(firstFit), ElapsedMillis(start), instance));

    start = std::chrono::steady_clock::now();
    std::optional<Solution> bestFit = BestFitDecreasing(instance.boxes, instance.container);
    double bestFitMillis = ElapsedMillis(start);
    std::optional<Solution> initial = bestFit;
    runs.push_back(MakeGreedyRun(Algorithm::BEST_FIT_DECREASING, std::move(bestFit),
                                 bestFitMillis, instance));

    if (!initial) {
        runs.push_back(AlgorithmRun{Algorithm::VNS, false, 0, 0, 0.0, {}, {}, {}, {}});
        runs.push_back(AlgorithmRun{Algorithm::TABU, false, 0, 0, 0.0, {}, {}, {}, {}});
        return runs;
    }

    SearchParams vnsParams = config.vnsParams;
    vnsParams.seed = seed;
    runs.push_back(MakeSearchRun(
        Algorithm::VNS, RunVns(*initial, instance.boxes, instance.container, vnsParams),
        instance));

    SearchParams tabuParams = config.tabuParams;
    tabuParams.seed = seed;
    runs.push_back(MakeSearchRun(
        Algorithm::TABU, RunTabu(*initial, instance.boxes, instance.container, tabuParams),
        instance));
    return runs;
}

static int VolumeLowerBound(const Instance& instance) {
    long long total = 0;
    for (const Box& box : instance.boxes) {
        total += BoxVolume(box);
    }
    long long capacity = ContainerVolume(instance.container);
    return static_cast<int>((total + capacity - 1) / capacity);
}

static std::ofstream OpenCsv(const ExperimentConfig& config, const std::string& name,
                             const std::string& header) {
    std::ofstream file(config.outputDirectory + "/" + name);
    file << header << "\n";
    file << std::fixed << std::setprecision(4);
    return file;
}

static void WriteLayout(const ExperimentConfig& config, const Instance& instance,
                        const std::vector<AlgorithmRun>& runs) {
    std::ofstream file = OpenCsv(config, "layout.csv",
                                 "algorithm,pallet,layer,z,boxId,x,y,width,depth,height");
    for (const AlgorithmRun& run : runs) {
        if (!run.produced) {
            continue;
        }
        int palletNumber = 0;
        for (const PalletState& pallet : run.solution.pallets) {
            ++palletNumber;
            int z = 0;
            int layerNumber = 0;
            for (const Layer& layer : pallet.layers) {
                ++layerNumber;
                for (const Placement& placement : layer.placements) {
                    file << AlgorithmCode(run.algorithm) << "," << palletNumber << ","
                         << layerNumber << "," << z << "," << placement.boxId << ","
                         << placement.x << "," << placement.y << "," << placement.width << ","
                         << placement.depth << "," << instance.boxes[placement.boxId].height
                         << "\n";
                }
                z += layer.height;
            }
        }
    }
}

void RunValidationExperiment(const ExperimentConfig& config) {
    std::ofstream file = OpenCsv(
        config, "validation.csv",
        "case,instance,algorithm,expectedFeasible,produced,violations,passed,pallets");

    auto record = [&](const std::string& caseCode, int index, const Instance& instance,
                      const std::vector<AlgorithmRun>& runs) {
        bool expected = FitsAll(instance.boxes, instance.container);
        for (const AlgorithmRun& run : runs) {
            bool passed = expected ? (run.produced && run.violations == 0) : !run.produced;
            file << caseCode << "," << index << "," << AlgorithmCode(run.algorithm) << ","
                 << (expected ? 1 : 0) << "," << (run.produced ? 1 : 0) << "," << run.violations
                 << "," << (passed ? 1 : 0) << "," << run.pallets << "\n";
        }
    };

    const ValidationCase cases[] = {ValidationCase::SIMPLE, ValidationCase::ROTATION,
                                    ValidationCase::MULTILAYER, ValidationCase::FRAGMENTATION,
                                    ValidationCase::INFEASIBLE};
    for (int caseIndex = 0; caseIndex < 5; ++caseIndex) {
        for (int index = 0; index < config.validationInstances; ++index) {
            unsigned long long seed = MixSeed(config.seed, caseIndex + 1, index);
            Instance instance = GenerateValidationInstance(cases[caseIndex], seed);
            record(ValidationCaseCode(cases[caseIndex]), index, instance,
                   RunAllAlgorithms(instance, config, seed));
        }
        std::cerr << "validation: " << ValidationCaseCode(cases[caseIndex]) << " done\n";
    }

    Instance example = ReportExampleInstance();
    std::vector<AlgorithmRun> exampleRuns = RunAllAlgorithms(example, config, config.seed);
    record("REPORT_EXAMPLE", 0, example, exampleRuns);
    WriteLayout(config, example, exampleRuns);
}

void RunQualityExperiment(const ExperimentConfig& config) {
    std::ofstream file = OpenCsv(config, "quality.csv",
                                 "scenario,size,instance,algorithm,pallets,lowerBound,millis,"
                                 "violations");
    const Scenario scenarios[] = {Scenario::HOMOGENEOUS, Scenario::HETEROGENEOUS,
                                  Scenario::FRAGMENTED};
    const int sizes[] = {50, 100, 200};

    for (int scenarioIndex = 0; scenarioIndex < 3; ++scenarioIndex) {
        for (int size : sizes) {
            for (int index = 0; index < config.qualityInstances; ++index) {
                unsigned long long seed = MixSeed(config.seed + 100, scenarioIndex * 1000 + size,
                                                  index);
                Instance instance = GenerateScenarioInstance(scenarios[scenarioIndex], size, seed);
                int bound = VolumeLowerBound(instance);
                for (const AlgorithmRun& run : RunAllAlgorithms(instance, config, seed)) {
                    file << ScenarioCode(scenarios[scenarioIndex]) << "," << size << "," << index
                         << "," << AlgorithmCode(run.algorithm) << "," << run.pallets << ","
                         << bound << "," << run.millis << "," << run.violations << "\n";
                }
            }
            std::cerr << "quality: " << ScenarioCode(scenarios[scenarioIndex]) << " n=" << size
                      << " done\n";
        }
    }
}

void RunScalabilityExperiment(const ExperimentConfig& config) {
    std::ofstream file = OpenCsv(config, "scalability.csv",
                                 "scenario,size,instance,algorithm,restart,pallets,lowerBound,"
                                 "millis,violations");
    const Scenario scenarios[] = {Scenario::HOMOGENEOUS, Scenario::HETEROGENEOUS,
                                  Scenario::FRAGMENTED};
    const int sizes[] = {20, 50, 100, 200, 400};

    for (int scenarioIndex = 0; scenarioIndex < 3; ++scenarioIndex) {
        for (int size : sizes) {
            for (int index = 0; index < config.scalabilityInstances; ++index) {
                unsigned long long seed = MixSeed(config.seed + 200, scenarioIndex * 1000 + size,
                                                  index);
                Instance instance = GenerateScenarioInstance(scenarios[scenarioIndex], size, seed);
                int bound = VolumeLowerBound(instance);
                for (const AlgorithmRun& run : RunAllAlgorithms(instance, config, seed)) {
                    std::string prefix = ScenarioCode(scenarios[scenarioIndex]) + "," +
                                         std::to_string(size) + "," + std::to_string(index) + "," +
                                         AlgorithmCode(run.algorithm) + ",";
                    if (run.restartPallets.empty()) {
                        file << prefix << 0 << "," << run.pallets << "," << bound << ","
                             << run.millis << "," << run.violations << "\n";
                        continue;
                    }
                    for (size_t restart = 0; restart < run.restartPallets.size(); ++restart) {
                        file << prefix << restart << "," << run.restartPallets[restart] << ","
                             << bound << "," << run.restartMillis[restart] << ","
                             << run.restartViolations[restart] << "\n";
                    }
                }
            }
            std::cerr << "scalability: " << ScenarioCode(scenarios[scenarioIndex])
                      << " n=" << size << " done\n";
        }
    }
}
