#pragma once

#include "common.h"
#include "message_queue.h"
#include "config_manager.h"
#include "dynamics_model.h"
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <nlohmann/json.hpp>

struct BallisticResult {
    SensorData source_data;
    ShotRecord shot_record;
    std::vector<TrajectoryPoint> full_trajectory;
    double launch_phase_contact_time_ms;
    double max_launch_acceleration_g;
    double mach_number_at_launch;
    double drag_coefficient_at_launch;
    std::string dynamics_version{"2.0_penalty_mach"};
};

class BallisticSimulator {
public:
    BallisticSimulator(const AppConfig& config,
                      std::shared_ptr<SensorQueue> input_queue,
                      std::vector<CrossbowType> crossbow_types);
    ~BallisticSimulator();

    bool start(int num_threads = 1);
    void stop();
    bool is_running() const;

    BallisticResult get_latest_result(uint32_t crossbow_id);
    std::vector<SensorData> get_trajectory_points(uint32_t crossbow_id, size_t max_points = 200);
    std::map<uint32_t, BallisticResult> get_all_latest_results();

    BallisticResult simulate_shot(uint32_t crossbow_id, double angle,
                                   double wind_speed = 0.0, double wind_direction = 0.0);

private:
    void worker_loop();
    CrossbowType* find_crossbow(uint32_t id);
    BallisticResult process_sensor_data(const SensorData& data);

    AppConfig config_;
    std::shared_ptr<SensorQueue> input_queue_;
    std::vector<CrossbowType> crossbow_types_;
    std::vector<std::thread> workers_;
    std::atomic<bool> running_;
    std::atomic<uint64_t> processed_count_;
    std::atomic<uint64_t> error_count_;

    std::mutex result_mutex_;
    std::map<uint32_t, BallisticResult> latest_results_;
    std::map<uint32_t, std::vector<SensorData>> recent_trajectories_;
};
