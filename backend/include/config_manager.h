#pragma once

#include "common.h"
#include "logger.h"
#include <string>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct AppConfig {
    int udp_port{9000};
    int http_port{8080};
    int metrics_port{9090};
    size_t queue_capacity{10000};
    int ballistic_threads{1};
    int accuracy_batch_size{100};
    int accuracy_analysis_interval_ms{5000};
    int alarm_check_interval_ms{1000};

    struct LogConfig {
        std::string level{"info"};
        std::string file{"crossbow_backend.log"};
        bool console{true};
    } log;

    struct MqttConfig {
        std::string broker_host{"localhost"};
        int broker_port{1883};
        std::string client_id{"crossbow_backend"};
        std::string topic_prefix{"crossbow/alerts"};
        std::string username{""};
        std::string password{""};
    } mqtt;

    struct AlertThresholds {
        double deformation_ratio{0.08};
        double tension_ratio{1.5};
        double temperature_c{60.0};
        int debounce_seconds{10};
    } thresholds;

    struct DynamicsParams {
        double gravity{9.81};
        double air_density{1.225};
        double arrow_ref_area{0.00025};
        double bow_efficiency{0.75};
        double sound_speed{343.0};
        double penalty_stiffness{8.0e5};
        double penalty_damping{5.0e3};
        double contact_threshold{0.001};
        double max_penalty_force{5000.0};
    } dynamics;

    struct ClickHouseConfig {
        std::string host{"localhost"};
        int port{9000};
        std::string database{"crossbow_sim"};
        std::string user{"default"};
        std::string password{""};
        int ttl_days{30};
    } clickhouse;
};

class ConfigManager {
public:
    static AppConfig load(const std::string& config_path) {
        AppConfig cfg;
        try {
            std::ifstream f(config_path);
            if (f.is_open()) {
                json j = json::parse(f);
                parse_json(j, cfg);
                LOG_INFO("[Config] Loaded from {}", config_path);
            } else {
                LOG_INFO("[Config] File not found, using defaults. Saving to {}", config_path);
                save_default(config_path, cfg);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("[Config] Load error: {}, using defaults", e.what());
        }
        return cfg;
    }

    static void save_default(const std::string& config_path, const AppConfig& cfg) {
        try {
            json j;
            to_json(j, cfg);
            std::ofstream f(config_path);
            f << j.dump(4) << std::endl;
        } catch (const std::exception& e) {
            LOG_ERROR("[Config] Save error: {}", e.what());
        }
    }

    static crossbow::LogLevel parse_log_level(const std::string& level_str) {
        if (level_str == "trace") return crossbow::LogLevel::Trace;
        if (level_str == "debug") return crossbow::LogLevel::Debug;
        if (level_str == "info") return crossbow::LogLevel::Info;
        if (level_str == "warn") return crossbow::LogLevel::Warn;
        if (level_str == "error") return crossbow::LogLevel::Error;
        if (level_str == "critical") return crossbow::LogLevel::Critical;
        return crossbow::LogLevel::Info;
    }

private:
    static void parse_json(const json& j, AppConfig& cfg) {
        cfg.udp_port = j.value("udp_port", cfg.udp_port);
        cfg.http_port = j.value("http_port", cfg.http_port);
        cfg.metrics_port = j.value("metrics_port", cfg.metrics_port);
        cfg.queue_capacity = j.value("queue_capacity", cfg.queue_capacity);
        cfg.ballistic_threads = j.value("ballistic_threads", cfg.ballistic_threads);
        cfg.accuracy_batch_size = j.value("accuracy_batch_size", cfg.accuracy_batch_size);
        cfg.accuracy_analysis_interval_ms = j.value("accuracy_analysis_interval_ms",
                                                     cfg.accuracy_analysis_interval_ms);
        cfg.alarm_check_interval_ms = j.value("alarm_check_interval_ms",
                                               cfg.alarm_check_interval_ms);

        if (j.contains("log")) {
            const auto& l = j["log"];
            cfg.log.level = l.value("level", cfg.log.level);
            cfg.log.file = l.value("file", cfg.log.file);
            cfg.log.console = l.value("console", cfg.log.console);
        }

        if (j.contains("mqtt")) {
            const auto& m = j["mqtt"];
            cfg.mqtt.broker_host = m.value("broker_host", cfg.mqtt.broker_host);
            cfg.mqtt.broker_port = m.value("broker_port", cfg.mqtt.broker_port);
            cfg.mqtt.client_id = m.value("client_id", cfg.mqtt.client_id);
            cfg.mqtt.topic_prefix = m.value("topic_prefix", cfg.mqtt.topic_prefix);
            cfg.mqtt.username = m.value("username", cfg.mqtt.username);
            cfg.mqtt.password = m.value("password", cfg.mqtt.password);
        }

        if (j.contains("thresholds")) {
            const auto& t = j["thresholds"];
            cfg.thresholds.deformation_ratio = t.value("deformation_ratio",
                                                        cfg.thresholds.deformation_ratio);
            cfg.thresholds.tension_ratio = t.value("tension_ratio",
                                                    cfg.thresholds.tension_ratio);
            cfg.thresholds.temperature_c = t.value("temperature_c",
                                                    cfg.thresholds.temperature_c);
            cfg.thresholds.debounce_seconds = t.value("debounce_seconds",
                                                       cfg.thresholds.debounce_seconds);
        }

        if (j.contains("dynamics")) {
            const auto& d = j["dynamics"];
            cfg.dynamics.gravity = d.value("gravity", cfg.dynamics.gravity);
            cfg.dynamics.air_density = d.value("air_density", cfg.dynamics.air_density);
            cfg.dynamics.arrow_ref_area = d.value("arrow_ref_area", cfg.dynamics.arrow_ref_area);
            cfg.dynamics.bow_efficiency = d.value("bow_efficiency", cfg.dynamics.bow_efficiency);
            cfg.dynamics.sound_speed = d.value("sound_speed", cfg.dynamics.sound_speed);
            cfg.dynamics.penalty_stiffness = d.value("penalty_stiffness",
                                                      cfg.dynamics.penalty_stiffness);
            cfg.dynamics.penalty_damping = d.value("penalty_damping",
                                                    cfg.dynamics.penalty_damping);
            cfg.dynamics.contact_threshold = d.value("contact_threshold",
                                                      cfg.dynamics.contact_threshold);
            cfg.dynamics.max_penalty_force = d.value("max_penalty_force",
                                                      cfg.dynamics.max_penalty_force);
        }

        if (j.contains("clickhouse")) {
            const auto& c = j["clickhouse"];
            cfg.clickhouse.host = c.value("host", cfg.clickhouse.host);
            cfg.clickhouse.port = c.value("port", cfg.clickhouse.port);
            cfg.clickhouse.database = c.value("database", cfg.clickhouse.database);
            cfg.clickhouse.user = c.value("user", cfg.clickhouse.user);
            cfg.clickhouse.password = c.value("password", cfg.clickhouse.password);
            cfg.clickhouse.ttl_days = c.value("ttl_days", cfg.clickhouse.ttl_days);
        }
    }

    static void to_json(json& j, const AppConfig& cfg) {
        j = json{
            {"udp_port", cfg.udp_port},
            {"http_port", cfg.http_port},
            {"metrics_port", cfg.metrics_port},
            {"queue_capacity", cfg.queue_capacity},
            {"ballistic_threads", cfg.ballistic_threads},
            {"accuracy_batch_size", cfg.accuracy_batch_size},
            {"accuracy_analysis_interval_ms", cfg.accuracy_analysis_interval_ms},
            {"alarm_check_interval_ms", cfg.alarm_check_interval_ms},
            {"log", {
                {"level", cfg.log.level},
                {"file", cfg.log.file},
                {"console", cfg.log.console}
            }},
            {"mqtt", {
                {"broker_host", cfg.mqtt.broker_host},
                {"broker_port", cfg.mqtt.broker_port},
                {"client_id", cfg.mqtt.client_id},
                {"topic_prefix", cfg.mqtt.topic_prefix},
                {"username", cfg.mqtt.username},
                {"password", cfg.mqtt.password}
            }},
            {"thresholds", {
                {"deformation_ratio", cfg.thresholds.deformation_ratio},
                {"tension_ratio", cfg.thresholds.tension_ratio},
                {"temperature_c", cfg.thresholds.temperature_c},
                {"debounce_seconds", cfg.thresholds.debounce_seconds}
            }},
            {"dynamics", {
                {"gravity", cfg.dynamics.gravity},
                {"air_density", cfg.dynamics.air_density},
                {"arrow_ref_area", cfg.dynamics.arrow_ref_area},
                {"bow_efficiency", cfg.dynamics.bow_efficiency},
                {"sound_speed", cfg.dynamics.sound_speed},
                {"penalty_stiffness", cfg.dynamics.penalty_stiffness},
                {"penalty_damping", cfg.dynamics.penalty_damping},
                {"contact_threshold", cfg.dynamics.contact_threshold},
                {"max_penalty_force", cfg.dynamics.max_penalty_force}
            }},
            {"clickhouse", {
                {"host", cfg.clickhouse.host},
                {"port", cfg.clickhouse.port},
                {"database", cfg.clickhouse.database},
                {"user", cfg.clickhouse.user},
                {"password", cfg.clickhouse.password},
                {"ttl_days", cfg.clickhouse.ttl_days}
            }}
        };
    }
};
