#pragma once

#include "common.h"
#include "message_queue.h"
#include "config_manager.h"
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <nlohmann/json.hpp>

class AlarmMqttService {
public:
    using AlertCallback = std::function<void(const Alert&)>;

    AlarmMqttService(const AppConfig& config,
                     std::shared_ptr<SensorQueue> input_queue,
                     std::shared_ptr<AlertQueue> output_queue,
                     std::vector<CrossbowType> crossbow_types);
    ~AlarmMqttService();

    bool start();
    void stop();
    bool is_running() const;

    void set_alert_callback(AlertCallback callback);

    std::vector<Alert> get_active_alerts(uint32_t crossbow_id = 0);
    bool check_and_alert(const SensorData& data);

private:
    void worker_loop();

    bool publish_alert_mqtt(const Alert& alert);
    CrossbowType* find_crossbow(uint32_t id);

    AppConfig config_;
    std::shared_ptr<SensorQueue> input_queue_;
    std::shared_ptr<AlertQueue> output_queue_;
    std::vector<CrossbowType> crossbow_types_;
    AlertCallback alert_callback_;

    std::thread worker_thread_;
    std::atomic<bool> running_;
    std::atomic<uint64_t> received_count_;
    std::atomic<uint64_t> alert_count_;

    std::mutex alert_mutex_;
    std::map<uint32_t, std::vector<Alert>> active_alerts_;
    std::map<uint32_t, double> last_alert_time_;
};
