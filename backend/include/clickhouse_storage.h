#pragma once

#include "common.h"
#include <string>
#include <vector>
#include <memory>

class ClickHouseStorage {
public:
    ClickHouseStorage(const std::string& host, int port, const std::string& database);
    ~ClickHouseStorage();

    bool connect();
    void disconnect();
    bool is_connected() const;

    bool insert_sensor_data(const SensorData& data);
    bool insert_sensor_batch(const std::vector<SensorData>& batch);
    bool insert_trajectory_data(uint32_t crossbow_id, const std::string& shot_id,
                                const std::vector<TrajectoryPoint>& trajectory);
    bool insert_shot_record(const ShotRecord& record);
    bool insert_alert(const Alert& alert);
    bool insert_accuracy_analysis(const AccuracyAnalysis& analysis);

    std::vector<SensorData> get_sensor_history(uint32_t crossbow_id, int hours = 24);
    std::vector<ShotRecord> get_shot_history(uint32_t crossbow_id, int limit = 100);
    std::vector<Alert> get_active_alerts();
    std::vector<CrossbowType> get_crossbow_types();
    AccuracyAnalysis get_latest_accuracy(uint32_t crossbow_id);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
