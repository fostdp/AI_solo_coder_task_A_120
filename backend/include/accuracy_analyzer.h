#pragma once

#include "common.h"
#include <vector>
#include <cmath>

class AccuracyAnalyzer {
public:
    AccuracyAnalyzer();

    AccuracyAnalysis analyze(uint32_t crossbow_id, const std::string& crossbow_name,
                             const std::vector<SensorData>& shots);

    double calculate_circular_error_probable(double std_x, double std_y);
    double calculate_optimal_sight_scale(const std::vector<SensorData>& shots);
    std::vector<std::pair<double, double>> calculate_sight_adjustments(
        const std::vector<SensorData>& shots);

private:
    double calculate_mean(const std::vector<double>& values);
    double calculate_std(const std::vector<double>& values, double mean);
    double normal_cdf(double x);
    double normal_quantile(double p);
};
