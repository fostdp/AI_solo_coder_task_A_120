#pragma once

#include "common.h"
#include <cmath>
#include <vector>

class DynamicsModel {
public:
    static constexpr double GRAVITY = 9.81;
    static constexpr double AIR_DENSITY = 1.225;
    static constexpr double ARROW_DRAG_COEFFICIENT = 1.2;
    static constexpr double ARROW_LIFT_COEFFICIENT = 0.3;
    static constexpr double ARROW_REFERENCE_AREA = 0.00025;
    static constexpr double BOW_EFFICIENCY = 0.75;

    DynamicsModel(const CrossbowType& crossbow);

    double calculate_initial_velocity(double draw_weight, double draw_length, double arrow_mass);
    double calculate_launch_acceleration(double string_tension, double string_stretch);
    std::vector<TrajectoryPoint> simulate_trajectory(
        double initial_velocity,
        double launch_angle,
        double wind_speed = 0.0,
        double wind_direction = 0.0,
        double time_step = 0.001
    );
    ShotRecord calculate_shot_results(const std::vector<TrajectoryPoint>& trajectory);
    double calculate_kinetic_energy(double velocity, double mass);
    std::pair<double, double> calculate_expected_spread(
        double range,
        double velocity_std_dev,
        double angle_std_dev
    );

private:
    CrossbowType crossbow_;
    Vec3 calculate_drag_force(const Vec3& velocity, double air_density);
    Vec3 calculate_lift_force(const Vec3& velocity, double air_density);
    Vec3 calculate_gravitational_force(double mass);
};
