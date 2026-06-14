#include "common.h"
#include "udp_receiver.h"
#include "clickhouse_storage.h"
#include "mqtt_alert_manager.h"
#include "dynamics_model.h"
#include "accuracy_analyzer.h"
#include "http_server.h"
#include <iostream>
#include <memory>
#include <atomic>
#include <signal.h>
#include <map>

#ifdef _WIN32
#include <windows.h>
#endif

std::atomic<bool> g_running(true);

struct AppState {
    std::shared_ptr<ClickHouseStorage> storage;
    std::shared_ptr<MqttAlertManager> alert_manager;
    std::shared_ptr<HttpServer> http_server;
    std::map<uint32_t, CrossbowType> crossbow_map;
    std::mutex mutex;
    std::function<void(const SensorData&)> data_callback;

    void on_sensor_data(const SensorData& data) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            storage->insert_sensor_data(data);
            if (http_server) {
                http_server->add_sensor_data(data);
            }

            auto it = crossbow_map.find(data.crossbow_id);
            if (it != crossbow_map.end()) {
                const CrossbowType& crossbow = it->second;
                alert_manager->check_and_alert(data, crossbow);

                DynamicsModel model(crossbow);
                auto trajectory = model.simulate_trajectory(
                    data.arrow_velocity, data.aim_angle,
                    data.wind_speed, data.wind_direction
                );
                auto record = model.calculate_shot_results(trajectory);
                record.timestamp = data.timestamp;
                record.crossbow_id = data.crossbow_id;
                storage->insert_trajectory_data(data.crossbow_id, record.shot_id, trajectory);
                storage->insert_shot_record(record);
            }
        }

        if (data_callback) {
            data_callback(data);
        }

        std::cout << "[DATA] " << data.crossbow_name
                  << " | v=" << data.arrow_velocity << " m/s"
                  << " | R=" << data.range << " m"
                  << " | T=" << data.bow_string_tension << " N"
                  << " | d=" << data.bow_arm_deformation << " m"
                  << std::endl;
    }
};

static void signal_handler(int sig) {
    std::cout << "\n[SYSTEM] Received shutdown signal..." << std::endl;
    g_running = false;
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    std::cout << "==================================================" << std::endl;
    std::cout << "  古代弩机弹射动力学仿真与精准度分析系统" << std::endl;
    std::cout << "  Crossbow Dynamics Simulation & Accuracy Analysis" << std::endl;
    std::cout << "==================================================" << std::endl;

    AppState app;

    app.storage = std::make_shared<ClickHouseStorage>("localhost", 9000, "crossbow_sim");
    if (!app.storage->connect()) {
        std::cerr << "[ERROR] Failed to connect to ClickHouse" << std::endl;
    }

    auto types = app.storage->get_crossbow_types();
    for (const auto& t : types) {
        app.crossbow_map[t.id] = t;
    }
    std::cout << "[SYSTEM] Loaded " << app.crossbow_map.size() << " crossbow types" << std::endl;

    app.alert_manager = std::make_shared<MqttAlertManager>(
        "localhost", 1883, "crossbow-backend", "crossbow/alerts"
    );
    app.alert_manager->connect();
    app.alert_manager->set_alert_callback([&app](const Alert& alert) {
        app.storage->insert_alert(alert);
    });

    app.http_server = std::make_shared<HttpServer>(8080, app.storage, app.alert_manager);

    UdpReceiver receiver(9000, [&app](const SensorData& data) {
        app.on_sensor_data(data);
    });

    if (!app.http_server->start()) {
        std::cerr << "[ERROR] Failed to start HTTP server" << std::endl;
        return 1;
    }

    if (!receiver.start()) {
        std::cerr << "[ERROR] Failed to start UDP receiver" << std::endl;
        return 1;
    }

    std::cout << "[SYSTEM] All services started successfully" << std::endl;
    std::cout << "  - UDP Receiver:   port 9000" << std::endl;
    std::cout << "  - HTTP API:       port 8080" << std::endl;
    std::cout << "  - MQTT Broker:    localhost:1883" << std::endl;
    std::cout << "  - ClickHouse:     localhost:9000" << std::endl;
    std::cout << std::endl;
    std::cout << "Press Ctrl+C to stop..." << std::endl;

    while (g_running) {
#ifdef _WIN32
        Sleep(500);
#else
        usleep(500000);
#endif
    }

    std::cout << "[SYSTEM] Shutting down..." << std::endl;
    receiver.stop();
    app.http_server->stop();
    app.alert_manager->disconnect();
    app.storage->disconnect();
    std::cout << "[SYSTEM] Shutdown complete" << std::endl;

    return 0;
}
