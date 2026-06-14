#include "accuracy_analyzer.h"
#include <algorithm>
#include <cmath>
#include <iostream>

AccuracyAnalyzer::AccuracyAnalyzer() {}

double AccuracyAnalyzer::calculate_mean(const std::vector<double>& values) {
    if (values.empty()) return 0.0;
    double sum = 0.0;
    for (double v : values) sum += v;
    return sum / values.size();
}

double AccuracyAnalyzer::calculate_std(const std::vector<double>& values, double mean) {
    if (values.size() < 2) return 0.0;
    double sum_sq = 0.0;
    for (double v : values) {
        double diff = v - mean;
        sum_sq += diff * diff;
    }
    return std::sqrt(sum_sq / (values.size() - 1));
}

double AccuracyAnalyzer::normal_cdf(double x) {
    return 0.5 * std::erfc(-x / std::sqrt(2.0));
}

double AccuracyAnalyzer::normal_quantile(double p) {
    if (p <= 0.0) return -1e300;
    if (p >= 1.0) return 1e300;

    static const double a[] = {-39.69683028665376, 220.9460984245205,
                               -275.9285104469687, 138.3577518672690,
                               -30.66479806614716, 2.506628277459239};
    static const double b[] = {-54.47609879822406, 161.5858368580409,
                               -155.6989798598866, 66.80131188771972,
                               -13.28068155288572};
    static const double c[] = {-0.007784894002430293, -0.3223964580411365,
                               -2.400758277161838, -2.549732539343734,
                               4.374664141464968, 2.938163982698783};
    static const double d[] = {0.007784695709041462, 0.3224671290700398,
                               2.445134137142996, 3.754408661907416};

    double plow = 0.02425, phigh = 1 - plow;
    double q, r;

    if (p < plow) {
        q = std::sqrt(-2 * std::log(p));
        return (((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) /
               ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1);
    } else if (p <= phigh) {
        q = p - 0.5;
        r = q * q;
        return (((((a[0] * r + a[1]) * r + a[2]) * r + a[3]) * r + a[4]) * r + a[5]) * q /
               (((((b[0] * r + b[1]) * r + b[2]) * r + b[3]) * r + b[4]) * r + 1);
    } else {
        q = std::sqrt(-2 * std::log(1 - p));
        return -(((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) /
               ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1);
    }
}

double AccuracyAnalyzer::calculate_circular_error_probable(double std_x, double std_y) {
    double sigma = std::sqrt(std_x * std_x + std_y * std_y);
    return sigma * std::sqrt(2 * std::log(2));
}

double AccuracyAnalyzer::calculate_optimal_sight_scale(const std::vector<SensorData>& shots) {
    if (shots.empty()) return 0.0;

    std::vector<double> ranges;
    std::vector<double> angles;

    for (const auto& shot : shots) {
        ranges.push_back(shot.range);
        angles.push_back(shot.aim_angle);
    }

    double mean_range = calculate_mean(ranges);
    double mean_angle = calculate_mean(angles);

    double numerator = 0.0, denominator = 0.0;
    for (size_t i = 0; i < shots.size(); i++) {
        double dr = ranges[i] - mean_range;
        double da = angles[i] - mean_angle;
        numerator += dr * da;
        denominator += dr * dr;
    }

    if (denominator < 1e-10) return 0.0;
    return numerator / denominator;
}

std::vector<std::pair<double, double>> AccuracyAnalyzer::calculate_sight_adjustments(
    const std::vector<SensorData>& shots) {
    std::vector<std::pair<double, double>> adjustments;

    if (shots.size() < 5) return adjustments;

    std::vector<double> range_bins = {50, 100, 150, 200, 250, 300, 400, 500, 600};

    for (double target_range : range_bins) {
        std::vector<SensorData> nearby_shots;
        for (const auto& shot : shots) {
            if (std::abs(shot.range - target_range) < 30.0) {
                nearby_shots.push_back(shot);
            }
        }

        if (nearby_shots.size() >= 3) {
            std::vector<double> sx, sy;
            for (const auto& s : nearby_shots) {
                sx.push_back(s.spread_x);
                sy.push_back(s.spread_y);
            }
            double mx = calculate_mean(sx);
            double my = calculate_mean(sy);
            adjustments.push_back({target_range, -0.5 * (mx + my)});
        }
    }

    return adjustments;
}

AccuracyAnalysis AccuracyAnalyzer::analyze(uint32_t crossbow_id, const std::string& crossbow_name,
                                           const std::vector<SensorData>& shots) {
    AccuracyAnalysis result;
    result.crossbow_id = crossbow_id;
    result.crossbow_name = crossbow_name;
    result.analysis_date = std::chrono::system_clock::now();
    result.total_shots = shots.size();

    if (shots.empty()) {
        result.mean_spread_x = 0;
        result.mean_spread_y = 0;
        result.std_spread_x = 0;
        result.std_spread_y = 0;
        result.circular_error_probable = 0;
        result.mean_velocity = 0;
        result.std_velocity = 0;
        result.mean_range = 0;
        result.optimal_sight_scale = 0;
        return result;
    }

    std::vector<double> spread_x, spread_y, velocities, ranges;
    for (const auto& shot : shots) {
        spread_x.push_back(shot.spread_x);
        spread_y.push_back(shot.spread_y);
        velocities.push_back(shot.arrow_velocity);
        ranges.push_back(shot.range);
    }

    result.mean_spread_x = calculate_mean(spread_x);
    result.mean_spread_y = calculate_mean(spread_y);
    result.std_spread_x = calculate_std(spread_x, result.mean_spread_x);
    result.std_spread_y = calculate_std(spread_y, result.mean_spread_y);
    result.circular_error_probable = calculate_circular_error_probable(
        result.std_spread_x, result.std_spread_y);
    result.mean_velocity = calculate_mean(velocities);
    result.std_velocity = calculate_std(velocities, result.mean_velocity);
    result.mean_range = calculate_mean(ranges);
    result.optimal_sight_scale = calculate_optimal_sight_scale(shots);
    result.sight_adjustments = calculate_sight_adjustments(shots);

    std::cout << "[Accuracy] Analysis complete for " << crossbow_name
              << ", shots: " << result.total_shots
              << ", CEP: " << result.circular_error_probable << " m" << std::endl;

    return result;
}
