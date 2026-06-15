#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <csignal>

#include "common.h"
#include "config_manager.h"
#include "message_queue.h"
#include "udp_receiver.h"
#include "ballistic_simulator.h"
#include "accuracy_analyzer_service.h"
#include "alarm_mqtt_service.h"
#include "http_server.h"
#include "clickhouse_storage.h"

namespace {
    volatile std::sig_atomic_t g_shutdown = 0;

    void signal_handler(int /*signal*/) {
        g_shutdown = 1;
    }

    std::vector<CrossbowType> get_default_crossbow_types() {
        return {
            {1, "秦弩", "秦朝", 150.0, 1.38, 1.42, 0.065, 150.0, 300.0},
            {2, "汉弩", "汉朝", 180.0, 1.45, 1.50, 0.068, 180.0, 350.0},
            {3, "魏武卒弩", "三国", 200.0, 1.50, 1.55, 0.070, 200.0, 380.0},
            {4, "诸葛连弩", "三国蜀", 120.0, 1.20, 1.25, 0.055, 100.0, 200.0},
            {5, "隋大弩", "隋朝", 220.0, 1.55, 1.60, 0.072, 220.0, 400.0},
            {6, "唐伏远弩", "唐朝", 250.0, 1.60, 1.65, 0.075, 250.0, 450.0},
            {7, "宋神臂弩", "宋朝", 350.0, 1.75, 1.80, 0.080, 300.0, 550.0},
            {8, "金铁鹞子弩", "金朝", 280.0, 1.65, 1.70, 0.077, 280.0, 480.0},
            {9, "元神风弩", "元朝", 300.0, 1.68, 1.73, 0.078, 290.0, 500.0},
            {10, "明三眼弩", "明朝", 260.0, 1.60, 1.65, 0.076, 260.0, 460.0}
        };
    }
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::string config_path = "config.json";
    if (argc > 1) {
        config_path = argv[1];
    }

    std::cout << "========================================" << std::endl;
    std::cout << "  弩机弹射动力学仿真系统 v2.0 (重构版)" << std::endl;
    std::cout << "  架构: 消息队列解耦 + 模块化设计" << std::endl;
    std::cout << "========================================" << std::endl;

    AppConfig config = ConfigManager::load(config_path);
    auto crossbow_types = get_default_crossbow_types();

    std::cout << "[Config] UDP port: " << config.udp_port << std::endl;
    std::cout << "[Config] HTTP port: " << config.http_port << std::endl;
    std::cout << "[Config] Queue capacity: " << config.queue_capacity << std::endl;
    std::cout << "[Config] Ballistic threads: " << config.ballistic_threads << std::endl;
    std::cout << "[Config] Crossbow types loaded: " << crossbow_types.size() << std::endl;

    auto sensor_queue_udp = std::make_shared<SensorQueue>(config.queue_capacity);
    auto sensor_queue_ballistic = std::make_shared<SensorQueue>(config.queue_capacity);
    auto sensor_queue_accuracy = std::make_shared<SensorQueue>(config.queue_capacity);
    auto sensor_queue_alarm = std::make_shared<SensorQueue>(config.queue_capacity);
    auto alert_queue = std::make_shared<AlertQueue>(config.queue_capacity);

    auto broadcaster = [&](const SensorData& data) {
        sensor_queue_ballistic->push(data);
        sensor_queue_accuracy->push(data);
        sensor_queue_alarm->push(data);
    };

    class BroadcastingReceiver {
    public:
        BroadcastingReceiver(int port, std::shared_ptr<SensorQueue> raw,
                             std::function<void(const SensorData&)> broadcast)
            : raw_queue_(raw), broadcast_(std::move(broadcast)) {
            impl_ = std::make_unique<UdpReceiver>(port, raw_queue_);
        }

        bool start() {
            if (!impl_->start()) return false;

            broadcast_thread_ = std::thread([this]() {
                SensorData data;
                while (running_) {
                    if (raw_queue_->wait_pop(data, 500)) {
                        broadcast_(data);
                    }
                }
            });
            running_ = true;
            return true;
        }

        void stop() {
            running_ = false;
            raw_queue_->stop();
            impl_->stop();
            if (broadcast_thread_.joinable()) {
                broadcast_thread_.join();
            }
        }

    private:
        std::unique_ptr<UdpReceiver> impl_;
        std::shared_ptr<SensorQueue> raw_queue_;
        std::function<void(const SensorData&)> broadcast_;
        std::thread broadcast_thread_;
        std::atomic<bool> running_{false};
    };

    BroadcastingReceiver receiver(config.udp_port, sensor_queue_udp, broadcaster);

    auto storage = std::make_shared<ClickHouseStorage>(
        config.clickhouse_host, config.clickhouse_port,
        config.clickhouse_db, config.clickhouse_user, config.clickhouse_password
    );
    storage->connect();
    storage->init_schema();
    for (const auto& cb : crossbow_types) {
        storage->insert_crossbow_type(cb);
    }

    auto ballistic = std::make_shared<BallisticSimulator>(
        config, sensor_queue_ballistic, crossbow_types
    );
    auto accuracy = std::make_shared<AccuracyAnalyzerService>(
        config, sensor_queue_accuracy
    );
    auto alarm = std::make_shared<AlarmMqttService>(
        config, sensor_queue_alarm, alert_queue, crossbow_types
    );

    auto http_server = std::make_shared<HttpServer>(
        config.http_port, storage, ballistic, accuracy, alarm
    );

    alarm->set_alert_callback([&storage](const Alert& alert) {
        storage->insert_alert(alert);
    });

    if (!receiver.start()) {
        std::cerr << "[Main] Failed to start UDP receiver" << std::endl;
        return 1;
    }
    if (!ballistic->start(config.ballistic_threads)) {
        std::cerr << "[Main] Failed to start ballistic simulator" << std::endl;
        return 1;
    }
    if (!accuracy->start()) {
        std::cerr << "[Main] Failed to start accuracy analyzer" << std::endl;
        return 1;
    }
    if (!alarm->start()) {
        std::cerr << "[Main] Failed to start alarm service" << std::endl;
        return 1;
    }
    if (!http_server->start()) {
        std::cerr << "[Main] Failed to start HTTP server" << std::endl;
        return 1;
    }

    std::cout << std::endl;
    std::cout << "[Main] All modules started successfully" << std::endl;
    std::cout << "[Main] Architecture flow:" << std::endl;
    std::cout << "       UDP Sensor Data" << std::endl;
    std::cout << "              ↓ (MessageQueue)" << std::endl;
    std::cout << "         ┌────┼────┐" << std::endl;
    std::cout << "    Ballistic  Accuracy  Alarm(MQTT)" << std::endl;
    std::cout << "              ↓           ↓" << std::endl;
    std::cout << "          HTTP API   ClickHouse" << std::endl;
    std::cout << std::endl;
    std::cout << "[Main] Press Ctrl+C to stop..." << std::endl;
    std::cout << std::endl;

    uint64_t tick_count = 0;
    while (!g_shutdown) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        tick_count++;
        if (tick_count % 60 == 0) {
            std::cout << "[Main] Heartbeat - UDP queue=" << sensor_queue_udp->size()
                      << ", Ballistic queue=" << sensor_queue_ballistic->size()
                      << ", Accuracy queue=" << sensor_queue_accuracy->size()
                      << ", Alarm queue=" << sensor_queue_alarm->size()
                      << ", Alert queue=" << alert_queue->size()
                      << std::endl;
        }
    }

    std::cout << std::endl;
    std::cout << "[Main] Shutdown signal received, stopping all modules..." << std::endl;

    http_server->stop();
    alarm->stop();
    accuracy->stop();
    ballistic->stop();
    receiver.stop();

    std::cout << "[Main] All modules stopped. Goodbye!" << std::endl;
    return 0;
}
