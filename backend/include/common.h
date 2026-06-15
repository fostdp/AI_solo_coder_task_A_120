#pragma once

#define _USE_MATH_DEFINES
#include <cmath>
#include <string>
#include <vector>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <random>
#include <algorithm>

struct SensorData {
    std::chrono::system_clock::time_point timestamp;
    uint32_t crossbow_id;
    std::string crossbow_name;
    double bow_string_tension;
    double bow_arm_deformation;
    double arrow_velocity;
    double range;
    double spread_x;
    double spread_y;
    double aim_angle;
    double temperature;
    double humidity;
    double wind_speed;
    double wind_direction;
};

struct Vec3 {
    double x;
    double y;
    double z;
};

struct TrajectoryPoint {
    double time_step;
    Vec3 position;
    Vec3 velocity;
    Vec3 acceleration;
    double drag_force;
    double lift_force;
};

struct CrossbowType {
    uint32_t id;
    std::string name;
    std::string dynasty;
    double draw_weight;
    double bow_length;
    double string_length;
    double arrow_mass;
    double effective_range;
    double max_range;
};

struct Alert {
    std::chrono::system_clock::time_point timestamp;
    uint32_t crossbow_id;
    std::string crossbow_name;
    std::string alert_type;
    std::string severity;
    std::string message;
    double threshold_value;
    double actual_value;
};

struct ShotRecord {
    std::chrono::system_clock::time_point timestamp;
    uint32_t crossbow_id;
    std::string shot_id;
    double initial_velocity;
    double launch_angle;
    double max_height;
    double flight_time;
    Vec3 impact_point;
    double impact_velocity;
    double kinetic_energy;
};

struct AccuracyAnalysis {
    uint32_t crossbow_id;
    std::string crossbow_name;
    std::chrono::system_clock::time_point analysis_date;
    uint32_t total_shots;
    double mean_spread_x;
    double mean_spread_y;
    double std_spread_x;
    double std_spread_y;
    double circular_error_probable;
    double mean_velocity;
    double std_velocity;
    double mean_range;
    double optimal_sight_scale;
    std::vector<std::pair<double, double>> sight_adjustments;
};

static std::string generate_uuid() {
    static std::mt19937 rng(std::random_device{}());
    static std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);
    uint64_t part1 = dist(rng);
    uint64_t part2 = dist(rng);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    oss << std::setw(8) << (part1 >> 32) << "-";
    oss << std::setw(4) << ((part1 >> 16) & 0xFFFF) << "-";
    oss << std::setw(4) << ((part1 & 0x0FFF) | 0x4000) << "-";
    oss << std::setw(4) << (((part2 >> 48) & 0x3FFF) | 0x8000) << "-";
    oss << std::setw(12) << (part2 & 0xFFFFFFFFFFFF);
    return oss.str();
}

static std::string format_timestamp(const std::chrono::system_clock::time_point& tp) {
    std::time_t time = std::chrono::system_clock::to_time_t(tp);
    std::tm* tm = std::localtime(&time);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()) % 1000;
    std::ostringstream oss;
    oss << std::put_time(tm, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

static double to_radians(double degrees) {
    return degrees * M_PI / 180.0;
}

static double to_degrees(double radians) {
    return radians * 180.0 / M_PI;
}
