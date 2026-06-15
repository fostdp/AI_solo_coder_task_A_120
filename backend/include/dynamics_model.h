#pragma once

#include "common.h"
#include <cmath>
#include <vector>
#include <functional>

class DynamicsModel {
public:
    static constexpr double GRAVITY = 9.81;
    static constexpr double AIR_DENSITY = 1.225;
    static constexpr double ARROW_REFERENCE_AREA = 0.00025;
    static constexpr double BOW_EFFICIENCY = 0.75;
    static constexpr double SOUND_SPEED = 343.0;

    static constexpr double PENALTY_STIFFNESS = 8.0e5;
    static constexpr double PENALTY_DAMPING = 5.0e3;
    static constexpr double CONTACT_THRESHOLD = 0.001;
    static constexpr double MAX_PENALTY_FORCE = 5000.0;

    DynamicsModel(const CrossbowType& crossbow);

    double calculate_initial_velocity(double draw_weight, double draw_length, double arrow_mass);
    double calculate_launch_acceleration(double string_tension, double string_stretch);

    struct LaunchPhaseResult {
        double initial_velocity;
        double max_string_acceleration;
        double contact_phase_time;
        std::vector<TrajectoryPoint> launch_trajectory;
    };

    LaunchPhaseResult simulate_launch_phase(
        double draw_weight,
        double draw_length,
        double launch_angle,
        double time_step = 0.0001
    );

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

    static double calculate_drag_coefficient(double mach_number);
    static double calculate_lift_coefficient(double mach_number, double angle_of_attack);

    Vec3 calculate_penalty_contact_force(
        double penetration_depth,
        double relative_velocity,
        double contact_normal
    );

    double get_string_position_at_time(double t, double draw_length);

    double get_bow_arm_bending_angle(double string_displacement);
    double get_bow_arm_deformation(double string_displacement);

private:
    CrossbowType crossbow_;

    Vec3 calculate_drag_force(const Vec3& velocity, double air_density);
    Vec3 calculate_lift_force(const Vec3& velocity, double air_density);
    Vec3 calculate_gravitational_force(double mass);

    static double smooth_step(double x, double edge0, double edge1);
    static double tanh_smooth(double x, double alpha = 10.0);

    static double subsonic_drag_coeff(double mach);
    static double transonic_drag_coeff(double mach);
    static double supersonic_drag_coeff(double mach);
};
