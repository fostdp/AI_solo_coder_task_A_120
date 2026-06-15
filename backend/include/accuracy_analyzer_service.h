#pragma once

#include "common.h"
#include "message_queue.h"
#include "config_manager.h"
#include "accuracy_analyzer.h"
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>

class AccuracyAnalyzerService {
public:
    AccuracyAnalyzerService(const AppConfig& config,
                            std::shared_ptr<SensorQueue> input_queue);
    ~AccuracyAnalyzerService();

    bool start();
    void stop();
    bool is_running() const;

    AccuracyAnalysis get_latest_analysis(uint32_t crossbow_id);
    std::map<uint32_t, AccuracyAnalysis> get_all_analyses();

    AccuracyAnalysis run_analysis_now(uint32_t crossbow_id,
                                       const std::string& crossbow_name);
    std::vector<SensorData> get_crossbow_data(uint32_t crossbow_id, size_t max_points = 2000);

private:
    void worker_loop();
    void periodic_analysis_loop();

    AppConfig config_;
    std::shared_ptr<SensorQueue> input_queue_;
    AccuracyAnalyzer analyzer_;

    std::thread worker_thread_;
    std::thread analysis_thread_;
    std::atomic<bool> running_;
    std::atomic<uint64_t> received_count_;
    std::atomic<uint64_t> analysis_count_;

    std::mutex data_mutex_;
    std::map<uint32_t, std::vector<SensorData>> crossbow_data_;
    std::map<uint32_t, std::string> crossbow_names_;

    std::mutex analysis_mutex_;
    std::map<uint32_t, AccuracyAnalysis> latest_analyses_;
};
