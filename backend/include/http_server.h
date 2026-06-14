#pragma once

#include "common.h"
#include "clickhouse_storage.h"
#include "dynamics_model.h"
#include "accuracy_analyzer.h"
#include "mqtt_alert_manager.h"
#include <string>
#include <memory>
#include <map>
#include <functional>

class HttpServer {
public:
    using RequestHandler = std::function<std::string(const std::map<std::string, std::string>& params)>;

    HttpServer(int port,
               std::shared_ptr<ClickHouseStorage> storage,
               std::shared_ptr<MqttAlertManager> alert_manager);
    ~HttpServer();

    bool start();
    void stop();
    bool is_running() const;

    void add_sensor_data(const SensorData& data);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    void register_routes();
    std::string handle_get_crossbow_types(const std::map<std::string, std::string>& params);
    std::string handle_get_sensor_data(const std::map<std::string, std::string>& params);
    std::string handle_get_shot_history(const std::map<std::string, std::string>& params);
    std::string handle_get_alerts(const std::map<std::string, std::string>& params);
    std::string handle_get_accuracy(const std::map<std::string, std::string>& params);
    std::string handle_simulate_shot(const std::map<std::string, std::string>& params);
    std::string handle_run_accuracy_analysis(const std::map<std::string, std::string>& params);
    std::string handle_resolve_alert(const std::map<std::string, std::string>& params);
};
