#pragma once

#include <string>

#include "LocalSearch.hpp"

struct ExperimentConfig
{
    std::string outputDirectory;
    int validationInstances;
    int qualityInstances;
    int scalabilityInstances;
    unsigned long long seed;
    SearchParams vnsParams;
    SearchParams tabuParams;
};

void RunValidationExperiment(const ExperimentConfig& config);
void RunQualityExperiment(const ExperimentConfig& config);
void RunScalabilityExperiment(const ExperimentConfig& config);
