#pragma once

#include "common.h"
#include <string>
#include <memory>
#include <functional>

class MqttAlertManager {
public:
    using AlertCallback = std::function<void(const Alert&)>;

    MqttAlertManager(const std::string& broker_host, int broker_port,
                     const std::string& client_id, const std::string& topic_prefix);
    ~MqttAlertManager();

    bool connect();
    void disconnect();
    bool is_connected() const;

    bool send_alert(const Alert& alert);
    bool check_and_alert(const SensorData& data, const CrossbowType& crossbow);

    void set_alert_callback(AlertCallback callback);

    static constexpr double DEFORMATION_THRESHOLD_RATIO = 0.08;
    static constexpr double TENSION_THRESHOLD_RATIO = 1.5;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
