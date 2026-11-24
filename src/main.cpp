#include <filesystem>
#include <iostream>
#include <map>
#include <string>

#include "Experiments.hpp"

static std::map<std::string, std::string> ParseArguments(int argc, char** argv) {
    std::map<std::string, std::string> values;
    for (int i = 1; i + 1 < argc; i += 2) {
        values[argv[i]] = argv[i + 1];
    }
    return values;
}

static int IntegerOption(const std::map<std::string, std::string>& values, const std::string& key,
                         int fallback) {
    auto found = values.find(key);
    return found == values.end() ? fallback : std::stoi(found->second);
}

int main(int argc, char** argv) {
    std::map<std::string, std::string> values = ParseArguments(argc, argv);

    ExperimentConfig config{};
    config.outputDirectory = values.count("--output") ? values["--output"] : "results";
    config.validationInstances = IntegerOption(values, "--validation-instances", 20);
    config.qualityInstances = IntegerOption(values, "--quality-instances", 10);
    config.scalabilityInstances = IntegerOption(values, "--scalability-instances", 5);
    config.seed = static_cast<unsigned long long>(IntegerOption(values, "--seed", 42));

    int restarts = IntegerOption(values, "--restarts", 10);
    config.vnsParams = SearchParams{IntegerOption(values, "--vns-iterations", 2000),
                                    IntegerOption(values, "--vns-stall", 600),
                                    IntegerOption(values, "--descent-steps", 60),
                                    0,
                                    0,
                                    restarts,
                                    config.seed};
    config.tabuParams = SearchParams{IntegerOption(values, "--tabu-iterations", 2000),
                                     IntegerOption(values, "--tabu-stall", 600),
                                     0,
                                     IntegerOption(values, "--tenure", 8),
                                     IntegerOption(values, "--neighbors", 60),
                                     restarts,
                                     config.seed};

    std::string experiment = values.count("--experiment") ? values["--experiment"] : "all";
    std::filesystem::create_directories(config.outputDirectory);

    if (experiment == "1" || experiment == "all") {
        RunValidationExperiment(config);
    }
    if (experiment == "2" || experiment == "all") {
        RunQualityExperiment(config);
    }
    if (experiment == "3" || experiment == "all") {
        RunScalabilityExperiment(config);
    }
    return 0;
}
