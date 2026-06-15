#include "clickhouse_storage.h"
#include "logger.h"
#include <iostream>
#include <sstream>

struct ClickHouseStorage::Impl {
    std::string host;
    int port;
    std::string database;
    std::string user;
    std::string password;
    bool connected = false;
    std::vector<CrossbowType> crossbow_types;
    std::vector<SensorData> sensor_data;
    std::vector<Alert> alerts;

    Impl(const std::string& h, int p, const std::string& db,
         const std::string& u = "default", const std::string& pw = "")
        : host(h), port(p), database(db), user(u), password(pw) {}

    std::string build_connection_string() const {
        std::ostringstream oss;
        oss << "tcp://" << host << ":" << port << "/" << database;
        return oss.str();
    }
};

ClickHouseStorage::ClickHouseStorage(const std::string& host, int port, const std::string& database,
                                     const std::string& user, const std::string& password)
    : impl_(std::make_unique<Impl>(host, port, database, user, password)) {}

ClickHouseStorage::~ClickHouseStorage() {
    disconnect();
}

bool ClickHouseStorage::connect() {
    LOG_INFO("[ClickHouse] Connecting to {}:{}/{}", impl_->host, impl_->port, impl_->database);
    impl_->connected = true;
    return true;
}

void ClickHouseStorage::disconnect() {
    impl_->connected = false;
    LOG_INFO("[ClickHouse] Disconnected");
}

bool ClickHouseStorage::is_connected() const {
    return impl_->connected;
}

bool ClickHouseStorage::init_schema() {
    LOG_INFO("[ClickHouse] Schema initialized (mock)");
    return true;
}

bool ClickHouseStorage::insert_crossbow_type(const CrossbowType& type) {
    impl_->crossbow_types.push_back(type);
    LOG_INFO("[ClickHouse] Insert crossbow type: {}", type.name);
    return true;
}

bool ClickHouseStorage::insert_sensor_data(const SensorData& data) {
    if (!impl_->connected) return false;
    impl_->sensor_data.push_back(data);
    if (impl_->sensor_data.size() > 50000) {
        impl_->sensor_data.erase(impl_->sensor_data.begin(), impl_->sensor_data.begin() + 10000);
    }
    LOG_INFO("[ClickHouse] Insert sensor - {} v={} R={}", data.crossbow_name, data.arrow_velocity, data.range);
    return true;
}

bool ClickHouseStorage::insert_sensor_batch(const std::vector<SensorData>& batch) {
    if (!impl_->connected) return false;
    for (const auto& data : batch) {
        insert_sensor_data(data);
    }
    return true;
}

bool ClickHouseStorage::insert_trajectory_data(uint32_t crossbow_id, const std::string& shot_id,
                                               const std::vector<TrajectoryPoint>& trajectory) {
    if (!impl_->connected) {
        LOG_INFO("[ClickHouse] Insert trajectory - crossbow_id: {}, shot_id: {}, points: {}", crossbow_id, shot_id, trajectory.size());
        return true;
    }
    return false;
}

bool ClickHouseStorage::insert_shot_record(const ShotRecord& record) {
    if (!impl_->connected) {
        double range = std::sqrt(record.impact_point.x * record.impact_point.x +
                                 record.impact_point.z * record.impact_point.z);
        LOG_INFO("[ClickHouse] Insert shot record - velocity: {}, range: {}", record.initial_velocity, range);
        return true;
    }
    return false;
}

bool ClickHouseStorage::insert_alert(const Alert& alert) {
    if (!impl_->connected) return false;
    impl_->alerts.push_back(alert);
    if (impl_->alerts.size() > 1000) {
        impl_->alerts.erase(impl_->alerts.begin(), impl_->alerts.begin() + 500);
    }
    LOG_WARN("[ClickHouse] Insert alert - {}, severity: {}: {}", alert.alert_type, alert.severity, alert.message);
    return true;
}

bool ClickHouseStorage::insert_accuracy_analysis(const AccuracyAnalysis& analysis) {
    if (!impl_->connected) {
        LOG_INFO("[ClickHouse] Insert accuracy analysis - crossbow_id: {}, CEP: {}", analysis.crossbow_name, analysis.circular_error_probable);
        return true;
    }
    return false;
}

std::vector<SensorData> ClickHouseStorage::get_sensor_history(uint32_t crossbow_id, int hours) {
    std::vector<SensorData> result;
    for (const auto& d : impl_->sensor_data) {
        if (d.crossbow_id == crossbow_id || crossbow_id == 0) {
            result.push_back(d);
        }
    }
    return result;
}

std::vector<ShotRecord> ClickHouseStorage::get_shot_history(uint32_t crossbow_id, int limit) {
    std::vector<ShotRecord> result;
    return result;
}

std::vector<Alert> ClickHouseStorage::get_active_alerts() {
    return impl_->alerts;
}

std::vector<CrossbowType> ClickHouseStorage::get_crossbow_types() {
    if (!impl_->crossbow_types.empty()) {
        return impl_->crossbow_types;
    }
    return {
        {1, "秦弩", "秦朝", 150.0, 1.38, 1.42, 0.065, 150.0, 300.0},
        {2, "汉弩（蹶张）", "汉朝", 180.0, 1.45, 1.48, 0.072, 180.0, 350.0},
        {3, "汉弩（腰引）", "汉朝", 250.0, 1.52, 1.55, 0.085, 220.0, 450.0},
        {4, "三国诸葛弩", "三国", 90.0, 0.95, 0.98, 0.045, 80.0, 150.0},
        {5, "晋代神弩", "晋朝", 300.0, 1.68, 1.72, 0.100, 250.0, 500.0},
        {6, "唐伏远弩", "唐朝", 200.0, 1.55, 1.58, 0.078, 200.0, 400.0},
        {7, "宋神臂弩", "宋朝", 350.0, 1.75, 1.78, 0.095, 300.0, 600.0},
        {8, "宋克敌弓", "宋朝", 280.0, 1.62, 1.65, 0.088, 260.0, 520.0},
        {9, "元复合弩", "元朝", 220.0, 1.50, 1.53, 0.080, 210.0, 420.0},
        {10, "明三眼弩", "明朝", 160.0, 1.42, 1.45, 0.068, 160.0, 320.0}
    };
}

AccuracyAnalysis ClickHouseStorage::get_latest_accuracy(uint32_t crossbow_id) {
    AccuracyAnalysis result;
    result.crossbow_id = crossbow_id;
    return result;
}
