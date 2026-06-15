#include "accuracy_analyzer_service.h"
#include "logger.h"
#include "metrics.h"
#include <iostream>

AccuracyAnalyzerService::AccuracyAnalyzerService(
    const AppConfig& config,
    std::shared_ptr<SensorQueue> input_queue)
    : config_(config), input_queue_(input_queue),
      running_(false), received_count_(0), analysis_count_(0) {}

AccuracyAnalyzerService::~AccuracyAnalyzerService() {
    stop();
}

bool AccuracyAnalyzerService::start() {
    if (running_) return true;
    running_ = true;
    worker_thread_ = std::thread(&AccuracyAnalyzerService::worker_loop, this);
    analysis_thread_ = std::thread(&AccuracyAnalyzerService::periodic_analysis_loop, this);
    LOG_INFO("[Accuracy] Analyzer service started");
    return true;
}

void AccuracyAnalyzerService::stop() {
    running_ = false;
    if (input_queue_) {
        input_queue_->stop();
    }
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    if (analysis_thread_.joinable()) {
        analysis_thread_.join();
    }
    LOG_INFO("[Accuracy] Service stopped. Received={}, analyses={}", received_count_, analysis_count_);
}

bool AccuracyAnalyzerService::is_running() const {
    return running_;
}

void AccuracyAnalyzerService::worker_loop() {
    SensorData data;
    while (running_) {
        try {
            if (!input_queue_->wait_pop(data, 500)) {
                continue;
            }
            {
                std::lock_guard<std::mutex> lock(data_mutex_);
                auto& vec = crossbow_data_[data.crossbow_id];
                vec.push_back(data);
                if (vec.size() > 5000) {
                    vec.erase(vec.begin(), vec.begin() + vec.size() - 5000);
                }
                crossbow_names_[data.crossbow_id] = data.crossbow_name;
            }
            received_count_++;
        } catch (const std::exception& e) {
            LOG_ERROR("[Accuracy] Worker error: {}", e.what());
        } catch (...) {
            LOG_ERROR("[Accuracy] Unknown worker error");
        }
    }
}

void AccuracyAnalyzerService::periodic_analysis_loop() {
    while (running_) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(config_.accuracy_analysis_interval_ms));
        if (!running_) break;

        std::vector<uint32_t> ids;
        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            for (const auto& kv : crossbow_data_) {
                if (kv.second.size() >= 10) {
                    ids.push_back(kv.first);
                }
            }
        }

        for (uint32_t id : ids) {
            try {
                std::string name;
                std::vector<SensorData> shots;
                {
                    std::lock_guard<std::mutex> lock(data_mutex_);
                    name = crossbow_names_[id];
                    shots = crossbow_data_[id];
                }
                auto analysis = analyzer_.analyze(id, name, shots);
                {
                    std::lock_guard<std::mutex> lock(analysis_mutex_);
                    latest_analyses_[id] = analysis;
                }
                analysis_count_++;
            } catch (const std::exception& e) {
                LOG_ERROR("[Accuracy] Analysis error for crossbow {}: {}", id, e.what());
            }
        }
        METRICS_ACCURACY_DONE();
    }
}

AccuracyAnalysis AccuracyAnalyzerService::get_latest_analysis(uint32_t crossbow_id) {
    std::lock_guard<std::mutex> lock(analysis_mutex_);
    auto it = latest_analyses_.find(crossbow_id);
    if (it != latest_analyses_.end()) return it->second;
    return {};
}

std::map<uint32_t, AccuracyAnalysis> AccuracyAnalyzerService::get_all_analyses() {
    std::lock_guard<std::mutex> lock(analysis_mutex_);
    return latest_analyses_;
}

AccuracyAnalysis AccuracyAnalyzerService::run_analysis_now(
    uint32_t crossbow_id, const std::string& crossbow_name
) {
    std::vector<SensorData> shots;
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        shots = crossbow_data_[crossbow_id];
    }
    auto analysis = analyzer_.analyze(crossbow_id, crossbow_name, shots);
    {
        std::lock_guard<std::mutex> lock(analysis_mutex_);
        latest_analyses_[crossbow_id] = analysis;
    }
    analysis_count_++;
    return analysis;
}

std::vector<SensorData> AccuracyAnalyzerService::get_crossbow_data(
    uint32_t crossbow_id, size_t max_points
) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = crossbow_data_.find(crossbow_id);
    if (it != crossbow_data_.end()) {
        size_t start = it->second.size() > max_points ? it->second.size() - max_points : 0;
        return std::vector<SensorData>(it->second.begin() + start, it->second.end());
    }
    return {};
}
