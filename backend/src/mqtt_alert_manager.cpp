#include "mqtt_alert_manager.h"
#include "logger.h"
#include "metrics.h"
#include <iostream>
#include <sstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct MqttAlertManager::Impl {
    std::string broker_host;
    int broker_port;
    std::string client_id;
    std::string topic_prefix;
    bool connected;
    AlertCallback callback;

    Impl(const std::string& h, int p, const std::string& cid, const std::string& tp)
        : broker_host(h), broker_port(p), client_id(cid), topic_prefix(tp), connected(false) {}
};

MqttAlertManager::MqttAlertManager(const std::string& broker_host, int broker_port,
                                   const std::string& client_id, const std::string& topic_prefix)
    : impl_(std::make_unique<Impl>(broker_host, broker_port, client_id, topic_prefix)) {}

MqttAlertManager::~MqttAlertManager() {
    disconnect();
}

bool MqttAlertManager::connect() {
    LOG_INFO("[MQTT] Connecting to broker {}:{} as {}", impl_->broker_host, impl_->broker_port, impl_->client_id);
    impl_->connected = true;
    return true;
}

void MqttAlertManager::disconnect() {
    impl_->connected = false;
    LOG_INFO("[MQTT] Disconnected");
}

bool MqttAlertManager::is_connected() const {
    return impl_->connected;
}

bool MqttAlertManager::send_alert(const Alert& alert) {
    json j;
    j["timestamp"] = format_timestamp(alert.timestamp);
    j["crossbow_id"] = alert.crossbow_id;
    j["crossbow_name"] = alert.crossbow_name;
    j["alert_type"] = alert.alert_type;
    j["severity"] = alert.severity;
    j["message"] = alert.message;
    j["threshold_value"] = alert.threshold_value;
    j["actual_value"] = alert.actual_value;

    std::string topic = impl_->topic_prefix + "/" + std::to_string(alert.crossbow_id);
    std::string payload = j.dump();

    LOG_INFO("[MQTT] Publish to {}: {}", topic, payload);
    METRICS_MQTT_PUBLISHED();

    if (impl_->callback) {
        impl_->callback(alert);
    }

    return true;
}

bool MqttAlertManager::check_and_alert(const SensorData& data, const CrossbowType& crossbow) {
    bool alerted = false;

    double deformation_threshold = crossbow.bow_length * DEFORMATION_THRESHOLD_RATIO;
    if (data.bow_arm_deformation > deformation_threshold) {
        Alert alert;
        alert.timestamp = data.timestamp;
        alert.crossbow_id = data.crossbow_id;
        alert.crossbow_name = data.crossbow_name;
        alert.alert_type = "ARM_CRACK_RISK";
        alert.severity = "CRITICAL";
        alert.message = "弩臂裂纹风险预警：形变量 " + std::to_string(data.bow_arm_deformation) +
                       " m 超过阈值 " + std::to_string(deformation_threshold) + " m";
        alert.threshold_value = deformation_threshold;
        alert.actual_value = data.bow_arm_deformation;
        send_alert(alert);
        METRICS_ALERT_FIRED();
        alerted = true;
    }

    double tension_threshold = crossbow.draw_weight * 9.81 * TENSION_THRESHOLD_RATIO;
    if (data.bow_string_tension > tension_threshold) {
        Alert alert;
        alert.timestamp = data.timestamp;
        alert.crossbow_id = data.crossbow_id;
        alert.crossbow_name = data.crossbow_name;
        alert.alert_type = "EXCESSIVE_TENSION";
        alert.severity = "WARNING";
        alert.message = "弓弦张力过高：张力 " + std::to_string(data.bow_string_tension) +
                       " N 超过阈值 " + std::to_string(tension_threshold) + " N";
        alert.threshold_value = tension_threshold;
        alert.actual_value = data.bow_string_tension;
        send_alert(alert);
        METRICS_ALERT_FIRED();
        alerted = true;
    }

    if (data.temperature > 60.0) {
        Alert alert;
        alert.timestamp = data.timestamp;
        alert.crossbow_id = data.crossbow_id;
        alert.crossbow_name = data.crossbow_name;
        alert.alert_type = "HIGH_TEMPERATURE";
        alert.severity = "WARNING";
        alert.message = "弩机温度过高：" + std::to_string(data.temperature) + "°C 超过安全阈值 60°C";
        alert.threshold_value = 60.0;
        alert.actual_value = data.temperature;
        send_alert(alert);
        METRICS_ALERT_FIRED();
        alerted = true;
    }

    return alerted;
}

void MqttAlertManager::set_alert_callback(AlertCallback callback) {
    impl_->callback = callback;
}
