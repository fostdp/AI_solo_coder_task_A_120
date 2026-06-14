#include "dynamics_model.h"
#include <cmath>
#include <iostream>

DynamicsModel::DynamicsModel(const CrossbowType& crossbow) : crossbow_(crossbow) {}

double DynamicsModel::calculate_initial_velocity(double draw_weight, double draw_length, double arrow_mass) {
    double total_energy = 0.5 * draw_weight * 9.81 * draw_length;
    double kinetic_energy = total_energy * BOW_EFFICIENCY;
    return std::sqrt(2.0 * kinetic_energy / arrow_mass);
}

double DynamicsModel::calculate_launch_acceleration(double string_tension, double string_stretch) {
    double force = string_tension * std::sin(string_stretch / crossbow_.string_length);
    return force / crossbow_.arrow_mass;
}

Vec3 DynamicsModel::calculate_drag_force(const Vec3& velocity, double air_density) {
    double speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z);
    if (speed < 0.001) return {0, 0, 0};

    double drag_magnitude = 0.5 * air_density * speed * speed * ARROW_DRAG_COEFFICIENT * ARROW_REFERENCE_AREA;
    return {
        -drag_magnitude * velocity.x / speed,
        -drag_magnitude * velocity.y / speed,
        -drag_magnitude * velocity.z / speed
    };
}

Vec3 DynamicsModel::calculate_lift_force(const Vec3& velocity, double air_density) {
    double speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z);
    if (speed < 0.001) return {0, 0, 0};

    double lift_magnitude = 0.5 * air_density * speed * speed * ARROW_LIFT_COEFFICIENT * ARROW_REFERENCE_AREA;

    double horizontal_speed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
    if (horizontal_speed < 0.001) return {0, 0, 0};

    return {
        -lift_magnitude * velocity.y * velocity.x / (speed * horizontal_speed),
        lift_magnitude * horizontal_speed / speed,
        -lift_magnitude * velocity.y * velocity.z / (speed * horizontal_speed)
    };
}

Vec3 DynamicsModel::calculate_gravitational_force(double mass) {
    return {0, -mass * GRAVITY, 0};
}

std::vector<TrajectoryPoint> DynamicsModel::simulate_trajectory(
    double initial_velocity,
    double launch_angle,
    double wind_speed,
    double wind_direction,
    double time_step
) {
    std::vector<TrajectoryPoint> trajectory;

    double angle_rad = to_radians(launch_angle);
    double wind_rad = to_radians(wind_direction);

    Vec3 position = {0, 1.5, 0};
    Vec3 velocity = {
        initial_velocity * std::cos(angle_rad),
        initial_velocity * std::sin(angle_rad),
        0
    };

    Vec3 wind_velocity = {
        wind_speed * std::cos(wind_rad),
        0,
        wind_speed * std::sin(wind_rad)
    };

    double t = 0;
    const int max_steps = 100000;

    for (int i = 0; i < max_steps; i++) {
        Vec3 relative_velocity = {
            velocity.x - wind_velocity.x,
            velocity.y - wind_velocity.y,
            velocity.z - wind_velocity.z
        };

        Vec3 drag = calculate_drag_force(relative_velocity, AIR_DENSITY);
        Vec3 lift = calculate_lift_force(relative_velocity, AIR_DENSITY);
        Vec3 gravity = calculate_gravitational_force(crossbow_.arrow_mass);

        Vec3 total_force = {
            drag.x + lift.x + gravity.x,
            drag.y + lift.y + gravity.y,
            drag.z + lift.z + gravity.z
        };

        Vec3 acceleration = {
            total_force.x / crossbow_.arrow_mass,
            total_force.y / crossbow_.arrow_mass,
            total_force.z / crossbow_.arrow_mass
        };

        TrajectoryPoint point;
        point.time_step = t;
        point.position = position;
        point.velocity = velocity;
        point.acceleration = acceleration;
        point.drag_force = std::sqrt(drag.x * drag.x + drag.y * drag.y + drag.z * drag.z);
        point.lift_force = std::sqrt(lift.x * lift.x + lift.y * lift.y + lift.z * lift.z);

        trajectory.push_back(point);

        velocity.x += acceleration.x * time_step;
        velocity.y += acceleration.y * time_step;
        velocity.z += acceleration.z * time_step;

        position.x += velocity.x * time_step;
        position.y += velocity.y * time_step;
        position.z += velocity.z * time_step;

        t += time_step;

        if (position.y <= 0 && i > 10) {
            position.y = 0;
            TrajectoryPoint final_point;
            final_point.time_step = t;
            final_point.position = position;
            final_point.velocity = velocity;
            final_point.acceleration = acceleration;
            final_point.drag_force = point.drag_force;
            final_point.lift_force = point.lift_force;
            trajectory.push_back(final_point);
            break;
        }
    }

    return trajectory;
}

ShotRecord DynamicsModel::calculate_shot_results(const std::vector<TrajectoryPoint>& trajectory) {
    ShotRecord record;
    record.shot_id = generate_uuid();
    record.timestamp = std::chrono::system_clock::now();

    if (trajectory.empty()) {
        record.max_height = 0;
        record.flight_time = 0;
        record.impact_point = {0, 0, 0};
        record.impact_velocity = 0;
        record.kinetic_energy = 0;
        return record;
    }

    record.initial_velocity = std::sqrt(
        trajectory[0].velocity.x * trajectory[0].velocity.x +
        trajectory[0].velocity.y * trajectory[0].velocity.y +
        trajectory[0].velocity.z * trajectory[0].velocity.z
    );

    record.max_height = 0;
    for (const auto& point : trajectory) {
        if (point.position.y > record.max_height) {
            record.max_height = point.position.y;
        }
    }

    const auto& last = trajectory.back();
    record.flight_time = last.time_step;
    record.impact_point = last.position;
    record.impact_velocity = std::sqrt(
        last.velocity.x * last.velocity.x +
        last.velocity.y * last.velocity.y +
        last.velocity.z * last.velocity.z
    );
    record.kinetic_energy = calculate_kinetic_energy(record.impact_velocity, crossbow_.arrow_mass);

    double horizontal_dist = std::sqrt(last.position.x * last.position.x + last.position.z * last.position.z);
    record.launch_angle = to_degrees(std::atan2(trajectory[0].velocity.y, std::sqrt(trajectory[0].velocity.x * trajectory[0].velocity.x + trajectory[0].velocity.z * trajectory[0].velocity.z)));

    return record;
}

double DynamicsModel::calculate_kinetic_energy(double velocity, double mass) {
    return 0.5 * mass * velocity * velocity;
}

std::pair<double, double> DynamicsModel::calculate_expected_spread(
    double range,
    double velocity_std_dev,
    double angle_std_dev
) {
    double range_std_dev = range * std::sqrt(
        std::pow(2.0 * velocity_std_dev / (crossbow_.arrow_mass * 0.01), 2) +
        std::pow(to_radians(angle_std_dev), 2)
    );
    double lateral_std_dev = range * to_radians(angle_std_dev * 0.5);

    return {range_std_dev * 0.3, lateral_std_dev * 0.3};
}
