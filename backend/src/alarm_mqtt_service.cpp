#include "alarm_mqtt_service.h"
#include "logger.h"
#include "metrics.h"
#include <iostream>
#include <sstream>

using json = nlohmann::json;

AlarmMqttService::AlarmMqttService(
    const AppConfig& config,
    std::shared_ptr<SensorQueue> input_queue,
    std::shared_ptr<AlertQueue> output_queue,
    std::vector<CrossbowType> crossbow_types)
    : config_(config), input_queue_(input_queue),
      output_queue_(output_queue),
      crossbow_types_(std::move(crossbow_types)),
      running_(false), received_count_(0), alert_count_(0) {}

AlarmMqttService::~AlarmMqttService() {
    stop();
}

bool AlarmMqttService::start() {
    if (running_) return true;
    running_ = true;
    worker_thread_ = std::thread(&AlarmMqttService::worker_loop, this);
    LOG_INFO("[Alarm] MQTT service started");
    return true;
}

void AlarmMqttService::stop() {
    running_ = false;
    if (input_queue_) {
        input_queue_->stop();
    }
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    LOG_INFO("[Alarm] Service stopped. Received={}, alerts={}", received_count_, alert_count_);
}

bool AlarmMqttService::is_running() const {
    return running_;
}

void AlarmMqttService::set_alert_callback(AlertCallback callback) {
    alert_callback_ = callback;
}

CrossbowType* AlarmMqttService::find_crossbow(uint32_t id) {
    for (auto& cb : crossbow_types_) {
        if (cb.id == id) return &cb;
    }
    return nullptr;
}

bool AlarmMqttService::check_and_alert(const SensorData& data) {
    bool alerted = false;
    CrossbowType* cb = find_crossbow(data.crossbow_id);
    if (!cb) return false;

    auto now = std::chrono::system_clock::now();
    double now_seconds = std::chrono::duration<double>(
        now.time_since_epoch()).count();

    double last_time = 0;
    {
        std::lock_guard<std::mutex> lock(alert_mutex_);
        last_time = last_alert_time_[data.crossbow_id];
    }
    double cooldown = 10.0;

    auto create_and_send = [&](const std::string& type, const std::string& severity,
                                const std::string& msg, double threshold, double actual) {
        Alert alert;
        alert.timestamp = data.timestamp;
        alert.crossbow_id = data.crossbow_id;
        alert.crossbow_name = data.crossbow_name;
        alert.alert_type = type;
        alert.severity = severity;
        alert.message = msg;
        alert.threshold_value = threshold;
        alert.actual_value = actual;

        if (output_queue_) {
            output_queue_->push(alert);
        }
        publish_alert_mqtt(alert);
        if (alert_callback_) {
            alert_callback_(alert);
        }

        {
            std::lock_guard<std::mutex> lock(alert_mutex_);
            active_alerts_[data.crossbow_id].push_back(alert);
            if (active_alerts_[data.crossbow_id].size() > 100) {
                active_alerts_[data.crossbow_id].erase(
                    active_alerts_[data.crossbow_id].begin(),
                    active_alerts_[data.crossbow_id].begin() +
                    active_alerts_[data.crossbow_id].size() - 100
                );
            }
            last_alert_time_[data.crossbow_id] = now_seconds;
        }

        alert_count_++;
        METRICS_ALERT_FIRED();
        alerted = true;
    };

    double deformation_threshold = cb->bow_length * config_.thresholds.deformation_ratio;
    if (data.bow_arm_deformation > deformation_threshold &&
        now_seconds - last_time > cooldown) {
        create_and_send("ARM_CRACK_RISK", "CRITICAL",
            "弩臂裂纹风险预警：形变量 " + std::to_string(data.bow_arm_deformation) +
            " m 超过阈值 " + std::to_string(deformation_threshold) + " m",
            deformation_threshold, data.bow_arm_deformation);
    }

    double tension_threshold = cb->draw_weight * 9.81 * config_.thresholds.tension_ratio;
    if (data.bow_string_tension > tension_threshold &&
        now_seconds - last_time > cooldown) {
        create_and_send("EXCESSIVE_TENSION", "WARNING",
            "弓弦张力过高：张力 " + std::to_string(data.bow_string_tension) +
            " N 超过阈值 " + std::to_string(tension_threshold) + " N",
            tension_threshold, data.bow_string_tension);
    }

    if (data.temperature > config_.thresholds.temperature_c &&
        now_seconds - last_time > cooldown) {
        create_and_send("HIGH_TEMPERATURE", "WARNING",
            "弩机温度过高：" + std::to_string(data.temperature) +
            "°C 超过安全阈值 " + std::to_string(config_.thresholds.temperature_c) + "°C",
            config_.thresholds.temperature_c, data.temperature);
    }

    return alerted;
}

bool AlarmMqttService::publish_alert_mqtt(const Alert& alert) {
    json j;
    j["timestamp"] = format_timestamp(alert.timestamp);
    j["crossbow_id"] = alert.crossbow_id;
    j["crossbow_name"] = alert.crossbow_name;
    j["alert_type"] = alert.alert_type;
    j["severity"] = alert.severity;
    j["message"] = alert.message;
    j["threshold_value"] = alert.threshold_value;
    j["actual_value"] = alert.actual_value;

    std::string topic = config_.mqtt.topic_prefix + "/" + std::to_string(alert.crossbow_id);
    std::string payload = j.dump();

    LOG_INFO("[Alarm][MQTT] Publish to {}: {}", topic, payload);
    METRICS_MQTT_PUBLISHED();
    return true;
}

void AlarmMqttService::worker_loop() {
    SensorData data;
    while (running_) {
        try {
            if (!input_queue_->wait_pop(data, 500)) {
                continue;
            }
            check_and_alert(data);
            received_count_++;
        } catch (const std::exception& e) {
            LOG_ERROR("[Alarm] Worker error: {}", e.what());
        } catch (...) {
            LOG_ERROR("[Alarm] Unknown worker error");
        }
    }
}

std::vector<Alert> AlarmMqttService::get_active_alerts(uint32_t crossbow_id) {
    std::lock_guard<std::mutex> lock(alert_mutex_);
    std::vector<Alert> result;
    if (crossbow_id == 0) {
        for (auto& kv : active_alerts_) {
            for (auto& a : kv.second) result.push_back(a);
        }
    } else {
        auto it = active_alerts_.find(crossbow_id);
        if (it != active_alerts_.end()) {
            result = it->second;
        }
    }
    return result;
}
