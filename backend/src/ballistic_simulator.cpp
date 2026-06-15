#include "ballistic_simulator.h"
#include "logger.h"
#include "metrics.h"
#include <iostream>
#include <chrono>

BallisticSimulator::BallisticSimulator(
    const AppConfig& config,
    std::shared_ptr<SensorQueue> input_queue,
    std::vector<CrossbowType> crossbow_types)
    : config_(config), input_queue_(input_queue),
      crossbow_types_(std::move(crossbow_types)),
      running_(false), processed_count_(0), error_count_(0) {}

BallisticSimulator::~BallisticSimulator() {
    stop();
}

bool BallisticSimulator::start(int num_threads) {
    if (running_) return true;
    running_ = true;
    for (int i = 0; i < num_threads; ++i) {
        workers_.emplace_back(&BallisticSimulator::worker_loop, this);
    }
    LOG_INFO("Simulator started with {} worker threads", num_threads);
    return true;
}

void BallisticSimulator::stop() {
    running_ = false;
    if (input_queue_) {
        input_queue_->stop();
    }
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
    workers_.clear();
    LOG_INFO("Simulator stopped. Processed={}, errors={}", processed_count_, error_count_);
}

bool BallisticSimulator::is_running() const {
    return running_;
}

CrossbowType* BallisticSimulator::find_crossbow(uint32_t id) {
    for (auto& cb : crossbow_types_) {
        if (cb.id == id) return &cb;
    }
    return nullptr;
}

BallisticResult BallisticSimulator::process_sensor_data(const SensorData& data) {
    BallisticResult result;
    result.source_data = data;

    CrossbowType* cb = find_crossbow(data.crossbow_id);
    if (!cb) {
        result.mach_number_at_launch = 0;
        result.drag_coefficient_at_launch = 0;
        result.launch_phase_contact_time_ms = 0;
        result.max_launch_acceleration_g = 0;
        return result;
    }

    try {
        DynamicsModel model(*cb);

        double draw_length = cb->bow_length * 0.6;
        auto launch_result = model.simulate_launch_phase(
            cb->draw_weight, draw_length, data.aim_angle
        );

        auto trajectory = model.simulate_trajectory(
            launch_result.initial_velocity, data.aim_angle,
            data.wind_speed, data.wind_direction
        );

        auto shot = model.calculate_shot_results(trajectory);

        result.shot_record = shot;
        result.full_trajectory = std::move(trajectory);
        result.launch_phase_contact_time_ms = launch_result.contact_phase_time * 1000.0;
        result.max_launch_acceleration_g =
            launch_result.max_string_acceleration / DynamicsModel::GRAVITY;
        result.mach_number_at_launch =
            launch_result.initial_velocity / DynamicsModel::SOUND_SPEED;
        result.drag_coefficient_at_launch =
            DynamicsModel::calculate_drag_coefficient(result.mach_number_at_launch);
    } catch (const std::exception& e) {
        error_count_++;
        LOG_ERROR("Simulation error: {}", e.what());
    }

    return result;
}

void BallisticSimulator::worker_loop() {
    SensorData data;
    while (running_) {
        try {
            if (!input_queue_->wait_pop(data, 500)) {
                continue;
            }

            auto start_time = std::chrono::high_resolution_clock::now();
            BallisticResult result = process_sensor_data(data);
            {
                std::lock_guard<std::mutex> lock(result_mutex_);
                latest_results_[data.crossbow_id] = result;
                auto& traj = recent_trajectories_[data.crossbow_id];
                traj.push_back(data);
                if (traj.size() > 1000) {
                    traj.erase(traj.begin());
                }
            }
            processed_count_++;
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
            METRICS_BALLISTIC_LATENCY(duration_ms);
            METRICS_BALLISTIC_DONE();
            if (processed_count_ % 50 == 0) {
                LOG_INFO("Processed {} shots, queue={}", processed_count_, input_queue_->size());
            }
        } catch (const std::exception& e) {
            error_count_++;
            LOG_ERROR("Worker error: {}", e.what());
        } catch (...) {
            error_count_++;
            LOG_ERROR("Unknown worker error");
        }
    }
}

BallisticResult BallisticSimulator::get_latest_result(uint32_t crossbow_id) {
    std::lock_guard<std::mutex> lock(result_mutex_);
    auto it = latest_results_.find(crossbow_id);
    if (it != latest_results_.end()) return it->second;
    return {};
}

std::vector<SensorData> BallisticSimulator::get_trajectory_points(
    uint32_t crossbow_id, size_t max_points
) {
    std::lock_guard<std::mutex> lock(result_mutex_);
    auto it = recent_trajectories_.find(crossbow_id);
    if (it != recent_trajectories_.end()) {
        size_t start = it->second.size() > max_points ? it->second.size() - max_points : 0;
        return std::vector<SensorData>(it->second.begin() + start, it->second.end());
    }
    return {};
}

std::map<uint32_t, BallisticResult> BallisticSimulator::get_all_latest_results() {
    std::lock_guard<std::mutex> lock(result_mutex_);
    return latest_results_;
}

BallisticResult BallisticSimulator::simulate_shot(
    uint32_t crossbow_id, double angle,
    double wind_speed, double wind_direction
) {
    SensorData data;
    data.timestamp = std::chrono::system_clock::now();
    data.crossbow_id = crossbow_id;
    data.aim_angle = angle;
    data.wind_speed = wind_speed;
    data.wind_direction = wind_direction;
    data.crossbow_name = "";

    CrossbowType* cb = find_crossbow(crossbow_id);
    if (cb) {
        data.crossbow_name = cb->name;
    }

    BallisticResult result = process_sensor_data(data);

    {
        std::lock_guard<std::mutex> lock(result_mutex_);
        latest_results_[crossbow_id] = result;
    }
    processed_count_++;

    return result;
}
