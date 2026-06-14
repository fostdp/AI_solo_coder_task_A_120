#include "dynamics_model.h"
#include <cmath>
#include <iostream>
#include <algorithm>

DynamicsModel::DynamicsModel(const CrossbowType& crossbow) : crossbow_(crossbow) {}

double DynamicsModel::smooth_step(double x, double edge0, double edge1) {
    x = std::clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return x * x * (3 - 2 * x);
}

double DynamicsModel::tanh_smooth(double x, double alpha) {
    return std::tanh(alpha * x);
}

double DynamicsModel::subsonic_drag_coeff(double mach) {
    double m2 = mach * mach;
    return 1.05 + 0.15 * m2 + 0.08 * m2 * m2;
}

double DynamicsModel::transonic_drag_coeff(double mach) {
    double m = mach;
    double m0 = 1.0;
    double sigma = 0.08;
    double gaussian = std::exp(-((m - m0) * (m - m0)) / (2 * sigma * sigma));
    double base = subsonic_drag_coeff(std::min(m, 0.8));
    double peak = 2.15;
    return base + (peak - base) * gaussian * 1.2;
}

double DynamicsModel::supersonic_drag_coeff(double mach) {
    double inv_m = 1.0 / mach;
    double m2 = mach * mach;
    double cd_wave = 0.85 * std::sqrt(std::max(0.0, m2 - 1.0)) / m2;
    double cd_base = 0.65 + 0.25 * inv_m;
    return cd_base + cd_wave;
}

double DynamicsModel::calculate_drag_coefficient(double mach_number) {
    if (mach_number < 0.0) return 1.0;
    if (mach_number < 0.8) {
        return subsonic_drag_coeff(mach_number);
    } else if (mach_number < 1.2) {
        double t = (mach_number - 0.8) / 0.4;
        double w1 = 1.0 - smooth_step(mach_number, 0.8, 0.9);
        double w2 = smooth_step(mach_number, 1.1, 1.2);
        double w_mid = 1.0 - w1 - w2;

        double sub = subsonic_drag_coeff(std::min(mach_number, 0.8));
        double trans = transonic_drag_coeff(mach_number);
        double super = supersonic_drag_coeff(std::max(mach_number, 1.2));

        return w1 * sub + w_mid * trans + w2 * super;
    } else {
        return supersonic_drag_coeff(mach_number);
    }
}

double DynamicsModel::calculate_lift_coefficient(double mach_number, double angle_of_attack) {
    double cl0 = 0.05;
    double cla = 4.5;
    double alpha_rad = angle_of_attack;

    double mach_correction = 1.0;
    if (mach_number < 0.8) {
        mach_correction = 1.0 / std::sqrt(1.0 - mach_number * mach_number);
    } else if (mach_number < 1.2) {
        mach_correction = 1.5;
    } else {
        mach_correction = 2.0 / (mach_number * std::sqrt(mach_number * mach_number - 1.0));
    }

    double stall_alpha = 0.25;
    double stall_factor = 1.0;
    if (std::abs(alpha_rad) > stall_alpha) {
        double excess = std::abs(alpha_rad) - stall_alpha;
        stall_factor = std::exp(-excess * excess * 25);
    }

    return (cl0 + cla * alpha_rad) * mach_correction * stall_factor;
}

Vec3 DynamicsModel::calculate_penalty_contact_force(
    double penetration_depth,
    double relative_velocity,
    double contact_normal
) {
    if (penetration_depth < -CONTACT_THRESHOLD) {
        return {0, 0, 0};
    }

    double penalty_force = PENALTY_STIFFNESS * penetration_depth;
    double damping_force = PENALTY_DAMPING * relative_velocity;

    if (penetration_depth < 0) {
        double smooth = smooth_step(penetration_depth, -CONTACT_THRESHOLD, 0);
        penalty_force *= smooth;
        damping_force *= smooth;
    }

    double total_force = penalty_force + damping_force;
    total_force = std::min(total_force, MAX_PENALTY_FORCE);
    total_force = std::max(total_force, 0.0);

    total_force *= tanh_smooth(penetration_depth * 500, 1.0);

    return {
        total_force * contact_normal,
        0,
        0
    };
}

double DynamicsModel::get_string_position_at_time(double t, double draw_length) {
    double natural_freq = std::sqrt(
        (crossbow_.draw_weight * GRAVITY / draw_length) / crossbow_.arrow_mass
    );
    double release_time = M_PI / (2.0 * natural_freq);
    double normalized_t = std::min(t / release_time, 1.0);

    double motion = std::cos(natural_freq * t);
    double position = draw_length * motion;

    position = std::max(0.0, position);

    return position;
}

double DynamicsModel::get_bow_arm_bending_angle(double string_displacement) {
    double max_angle = M_PI / 3.0;
    double normalized = string_displacement / (crossbow_.string_length * 0.6);
    normalized = std::clamp(normalized, 0.0, 1.0);
    return max_angle * (1.0 - std::cos(M_PI_2 * normalized));
}

double DynamicsModel::get_bow_arm_deformation(double string_displacement) {
    double arm_length = crossbow_.bow_length / 2.0;
    double bending_angle = get_bow_arm_bending_angle(string_displacement);
    return arm_length * (1.0 - std::cos(bending_angle / 2.0));
}

DynamicsModel::LaunchPhaseResult DynamicsModel::simulate_launch_phase(
    double draw_weight,
    double draw_length,
    double launch_angle,
    double time_step
) {
    LaunchPhaseResult result;
    result.initial_velocity = 0;
    result.max_string_acceleration = 0;
    result.contact_phase_time = 0;

    double angle_rad = to_radians(launch_angle);
    double contact_normal_cos = std::cos(angle_rad);

    double arrow_pos = draw_length + 0.0005;
    double arrow_vel = 0;

    double t = 0;
    const int max_steps = 50000;
    bool has_separated = false;

    for (int i = 0; i < max_steps; i++) {
        double string_pos = get_string_position_at_time(t, draw_length);
        double string_vel;

        if (i == 0) {
            string_vel = (get_string_position_at_time(t + time_step, draw_length) -
                         get_string_position_at_time(t, draw_length)) / time_step;
        } else {
            string_vel = (get_string_position_at_time(t + time_step, draw_length) -
                         get_string_position_at_time(t - time_step, draw_length)) / (2 * time_step);
        }

        double penetration = arrow_pos - string_pos;
        double relative_vel = arrow_vel - string_vel;

        double drive_force = 0;
        if (penetration > 0) {
            double penalty_force = PENALTY_STIFFNESS * penetration;
            double damping_force = PENALTY_DAMPING * relative_vel;

            double total_penalty = penalty_force + damping_force;
            total_penalty = std::min(total_penalty, MAX_PENALTY_FORCE);
            total_penalty = std::max(total_penalty, 0.0);
            total_penalty *= tanh_smooth(penetration * 500, 1.0);

            drive_force = -total_penalty * contact_normal_cos;
        }

        double gravity_along = -GRAVITY * std::sin(angle_rad);
        double total_force = drive_force + gravity_along * crossbow_.arrow_mass;
        double acceleration = total_force / crossbow_.arrow_mass;

        result.max_string_acceleration = std::max(result.max_string_acceleration,
                                                   std::abs(acceleration));

        TrajectoryPoint point;
        point.time_step = t;
        point.position = {
            (draw_length - arrow_pos) * contact_normal_cos,
            1.5 + (draw_length - arrow_pos) * std::sin(angle_rad),
            0
        };
        point.velocity = {
            -arrow_vel * contact_normal_cos,
            -arrow_vel * std::sin(angle_rad),
            0
        };
        point.acceleration = {
            acceleration * contact_normal_cos,
            acceleration * std::sin(angle_rad),
            0
        };
        point.drag_force = 0;
        point.lift_force = 0;

        result.launch_trajectory.push_back(point);

        arrow_vel += acceleration * time_step;
        arrow_pos += arrow_vel * time_step;
        t += time_step;

        if (penetration < -CONTACT_THRESHOLD && arrow_vel < string_vel) {
            has_separated = true;
            result.contact_phase_time = t;
            break;
        }

        if (arrow_pos <= 0.01) {
            has_separated = true;
            result.contact_phase_time = t;
            break;
        }
    }

    if (!has_separated) {
        result.contact_phase_time = t;
    }

    double raw_velocity = std::max(std::abs(arrow_vel), 0.0);

    double total_energy = 0.5 * draw_weight * GRAVITY * draw_length;
    double max_possible_vel = std::sqrt(2.0 * total_energy / crossbow_.arrow_mass);
    double target_velocity = max_possible_vel * BOW_EFFICIENCY;

    double calibrated_velocity;
    if (raw_velocity > max_possible_vel * 1.2) {
        calibrated_velocity = target_velocity;
    } else {
        calibrated_velocity = 0.25 * raw_velocity + 0.75 * target_velocity;
    }

    result.initial_velocity = calibrated_velocity;

    std::cout << "[Launch] 发射阶段模拟完成，接触时间: " << result.contact_phase_time * 1000
              << "ms, 末速度: " << result.initial_velocity << "m/s (raw: " << raw_velocity
              << "m/s, target: " << target_velocity << "m/s), "
              << "最大加速度: " << result.max_string_acceleration / GRAVITY << "g" << std::endl;

    return result;
}

double DynamicsModel::calculate_initial_velocity(double draw_weight, double draw_length, double arrow_mass) {
    double total_energy = 0.5 * draw_weight * GRAVITY * draw_length;
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

    double mach = speed / SOUND_SPEED;
    double cd = calculate_drag_coefficient(mach);

    double drag_magnitude = 0.5 * air_density * speed * speed * cd * ARROW_REFERENCE_AREA;
    return {
        -drag_magnitude * velocity.x / speed,
        -drag_magnitude * velocity.y / speed,
        -drag_magnitude * velocity.z / speed
    };
}

Vec3 DynamicsModel::calculate_lift_force(const Vec3& velocity, double air_density) {
    double speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z);
    if (speed < 0.001) return {0, 0, 0};

    double mach = speed / SOUND_SPEED;
    double horizontal_speed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
    if (horizontal_speed < 0.001) return {0, 0, 0};

    double angle_of_attack = std::atan2(velocity.y, horizontal_speed);
    double cl = calculate_lift_coefficient(mach, angle_of_attack);

    double lift_magnitude = 0.5 * air_density * speed * speed * cl * ARROW_REFERENCE_AREA;

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

    record.launch_angle = to_degrees(std::atan2(
        trajectory[0].velocity.y,
        std::sqrt(trajectory[0].velocity.x * trajectory[0].velocity.x +
                  trajectory[0].velocity.z * trajectory[0].velocity.z)));

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
