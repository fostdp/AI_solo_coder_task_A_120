#include "http_server.h"
#include <iostream>
#include <sstream>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

using json = nlohmann::json;

struct HttpServer::Impl {
    int port;
    std::shared_ptr<ClickHouseStorage> storage;
    std::shared_ptr<BallisticSimulator> ballistic;
    std::shared_ptr<AccuracyAnalyzerService> accuracy;
    std::shared_ptr<AlarmMqttService> alarm;
    std::atomic<bool> running;
    std::thread server_thread;
#ifdef _WIN32
    SOCKET server_fd;
    WSADATA wsa_data;
#else
    int server_fd;
#endif

    Impl(int p, std::shared_ptr<ClickHouseStorage> s,
         std::shared_ptr<BallisticSimulator> b,
         std::shared_ptr<AccuracyAnalyzerService> a,
         std::shared_ptr<AlarmMqttService> al)
        : port(p), storage(s), ballistic(b), accuracy(a), alarm(al),
          running(false), server_fd(-1) {}
};

HttpServer::HttpServer(int port,
                       std::shared_ptr<ClickHouseStorage> storage,
                       std::shared_ptr<BallisticSimulator> ballistic,
                       std::shared_ptr<AccuracyAnalyzerService> accuracy,
                       std::shared_ptr<AlarmMqttService> alarm)
    : impl_(std::make_unique<Impl>(port, storage, ballistic, accuracy, alarm)) {}

HttpServer::~HttpServer() {
    stop();
}

std::string HttpServer::handle_get_crossbow_types(const std::map<std::string, std::string>& params) {
    auto types = impl_->storage->get_crossbow_types();
    json j = json::array();
    for (const auto& t : types) {
        j.push_back({
            {"id", t.id},
            {"name", t.name},
            {"dynasty", t.dynasty},
            {"draw_weight", t.draw_weight},
            {"bow_length", t.bow_length},
            {"string_length", t.string_length},
            {"arrow_mass", t.arrow_mass},
            {"effective_range", t.effective_range},
            {"max_range", t.max_range}
        });
    }
    return j.dump();
}

std::string HttpServer::handle_get_sensor_data(const std::map<std::string, std::string>& params) {
    json j = json::array();
    auto it = params.find("crossbow_id");
    if (it != params.end() && impl_->accuracy) {
        uint32_t id = std::stoul(it->second);
        auto limit_it = params.find("limit");
        size_t limit = limit_it != params.end() ? std::stoul(limit_it->second) : 100;
        auto data = impl_->accuracy->get_crossbow_data(id, limit);
        for (const auto& d : data) {
            j.push_back({
                {"timestamp", format_timestamp(d.timestamp)},
                {"crossbow_id", d.crossbow_id},
                {"crossbow_name", d.crossbow_name},
                {"bow_string_tension", d.bow_string_tension},
                {"bow_arm_deformation", d.bow_arm_deformation},
                {"arrow_velocity", d.arrow_velocity},
                {"range", d.range},
                {"spread_x", d.spread_x},
                {"spread_y", d.spread_y},
                {"aim_angle", d.aim_angle},
                {"temperature", d.temperature},
                {"humidity", d.humidity},
                {"wind_speed", d.wind_speed},
                {"wind_direction", d.wind_direction}
            });
        }
    }
    return j.dump();
}

std::string HttpServer::handle_get_shot_history(const std::map<std::string, std::string>& params) {
    json j = json::array();
    auto it = params.find("crossbow_id");
    if (it != params.end() && impl_->accuracy) {
        uint32_t id = std::stoul(it->second);
        auto limit_it = params.find("limit");
        size_t limit = limit_it != params.end() ? std::stoul(limit_it->second) : 50;
        auto data = impl_->accuracy->get_crossbow_data(id, limit);
        for (const auto& d : data) {
            j.push_back({
                {"timestamp", format_timestamp(d.timestamp)},
                {"crossbow_id", d.crossbow_id},
                {"crossbow_name", d.crossbow_name},
                {"velocity", d.arrow_velocity},
                {"range", d.range},
                {"aim_angle", d.aim_angle},
                {"spread", std::sqrt(d.spread_x * d.spread_x + d.spread_y * d.spread_y)}
            });
        }
    }
    return j.dump();
}

std::string HttpServer::handle_get_alerts(const std::map<std::string, std::string>& params) {
    json j = json::array();
    if (!impl_->alarm) return j.dump();

    uint32_t crossbow_id = 0;
    auto it = params.find("crossbow_id");
    if (it != params.end()) crossbow_id = std::stoul(it->second);

    auto alerts = impl_->alarm->get_active_alerts(crossbow_id);
    for (const auto& a : alerts) {
        j.push_back({
            {"timestamp", format_timestamp(a.timestamp)},
            {"crossbow_id", a.crossbow_id},
            {"crossbow_name", a.crossbow_name},
            {"alert_type", a.alert_type},
            {"severity", a.severity},
            {"message", a.message},
            {"threshold_value", a.threshold_value},
            {"actual_value", a.actual_value}
        });
    }
    return j.dump();
}

std::string HttpServer::handle_get_accuracy(const std::map<std::string, std::string>& params) {
    auto it = params.find("crossbow_id");
    if (it != params.end() && impl_->accuracy) {
        uint32_t id = std::stoul(it->second);
        auto analysis = impl_->accuracy->get_latest_analysis(id);

        json adjustments = json::array();
        for (const auto& adj : analysis.sight_adjustments) {
            adjustments.push_back({
                {"range", adj.first},
                {"adjustment", adj.second}
            });
        }

        json j = {
            {"crossbow_id", analysis.crossbow_id},
            {"crossbow_name", analysis.crossbow_name},
            {"total_shots", analysis.total_shots},
            {"mean_spread_x", analysis.mean_spread_x},
            {"mean_spread_y", analysis.mean_spread_y},
            {"std_spread_x", analysis.std_spread_x},
            {"std_spread_y", analysis.std_spread_y},
            {"circular_error_probable", analysis.circular_error_probable},
            {"mean_velocity", analysis.mean_velocity},
            {"std_velocity", analysis.std_velocity},
            {"mean_range", analysis.mean_range},
            {"optimal_sight_scale", analysis.optimal_sight_scale},
            {"sight_adjustments", adjustments}
        };
        return j.dump();
    }
    return "{}";
}

std::string HttpServer::handle_simulate_shot(const std::map<std::string, std::string>& params) {
    auto it = params.find("crossbow_id");
    if (it == params.end() || !impl_->ballistic) return "{\"error\":\"missing crossbow_id\"}";

    uint32_t id = std::stoul(it->second);
    double angle = 15.0;
    double wind_speed = 0.0;
    double wind_direction = 0.0;

    auto angle_it = params.find("angle");
    if (angle_it != params.end()) angle = std::stod(angle_it->second);
    auto wind_it = params.find("wind_speed");
    if (wind_it != params.end()) wind_speed = std::stod(wind_it->second);
    auto wd_it = params.find("wind_direction");
    if (wd_it != params.end()) wind_direction = std::stod(wd_it->second);

    auto result = impl_->ballistic->simulate_shot(id, angle, wind_speed, wind_direction);

    json traj = json::array();
    const auto& trajectory = result.full_trajectory;
    size_t step = std::max((size_t)1, trajectory.size() / 200);
    for (size_t i = 0; i < trajectory.size(); i += step) {
        const auto& p = trajectory[i];
        traj.push_back({
            {"t", p.time_step},
            {"x", p.position.x},
            {"y", p.position.y},
            {"z", p.position.z},
            {"vx", p.velocity.x},
            {"vy", p.velocity.y},
            {"vz", p.velocity.z}
        });
    }
    if (trajectory.size() > 0) {
        const auto& p = trajectory.back();
        traj.push_back({
            {"t", p.time_step},
            {"x", p.position.x},
            {"y", p.position.y},
            {"z", p.position.z},
            {"vx", p.velocity.x},
            {"vy", p.velocity.y},
            {"vz", p.velocity.z}
        });
    }

    json j = {
        {"shot_id", result.shot_record.shot_id},
        {"initial_velocity", result.shot_record.initial_velocity},
        {"launch_angle", result.shot_record.launch_angle},
        {"max_height", result.shot_record.max_height},
        {"flight_time", result.shot_record.flight_time},
        {"impact_point", {
            {"x", result.shot_record.impact_point.x},
            {"y", result.shot_record.impact_point.y},
            {"z", result.shot_record.impact_point.z}
        }},
        {"impact_velocity", result.shot_record.impact_velocity},
        {"kinetic_energy", result.shot_record.kinetic_energy},
        {"trajectory", traj},
        {"mach_number", result.mach_number_at_launch},
        {"drag_coefficient", result.drag_coefficient_at_launch},
        {"contact_phase_time_ms", result.launch_phase_contact_time_ms},
        {"max_launch_acceleration_g", result.max_launch_acceleration_g},
        {"dynamics_version", result.dynamics_version}
    };
    return j.dump();
}

std::string HttpServer::handle_run_accuracy_analysis(const std::map<std::string, std::string>& params) {
    auto it = params.find("crossbow_id");
    if (it == params.end() || !impl_->accuracy) return "{\"error\":\"missing crossbow_id\"}";

    uint32_t id = std::stoul(it->second);
    auto data = impl_->accuracy->get_crossbow_data(id, 5000);
    std::string name;
    if (!data.empty()) name = data[0].crossbow_name;
    impl_->accuracy->run_analysis_now(id, name);
    return handle_get_accuracy(params);
}

std::string HttpServer::handle_resolve_alert(const std::map<std::string, std::string>& params) {
    return "{\"status\":\"ok\"}";
}

std::string HttpServer::handle_get_system_status(const std::map<std::string, std::string>& params) {
    json j = {
        {"status", "running"},
        {"version", "2.0_refactored"},
        {"architecture", "udp_receiver → message_queue → (ballistic_simulator | accuracy_analyzer | alarm_mqtt)"},
        {"modules", {
            {"udp_receiver", impl_->ballistic != nullptr},
            {"ballistic_simulator", impl_->ballistic != nullptr},
            {"accuracy_analyzer", impl_->accuracy != nullptr},
            {"alarm_mqtt", impl_->alarm != nullptr}
        }}
    };
    return j.dump();
}

std::string HttpServer::handle_get_ballistic_result(const std::map<std::string, std::string>& params) {
    if (!impl_->ballistic) return "{}";
    auto it = params.find("crossbow_id");
    if (it == params.end()) {
        auto all = impl_->ballistic->get_all_latest_results();
        json j = json::object();
        for (auto& kv : all) {
            j[std::to_string(kv.first)] = {
                {"initial_velocity", kv.second.shot_record.initial_velocity},
                {"range", kv.second.source_data.range},
                {"mach_number", kv.second.mach_number_at_launch},
                {"drag_coefficient", kv.second.drag_coefficient_at_launch},
                {"max_launch_acceleration_g", kv.second.max_launch_acceleration_g},
                {"contact_phase_time_ms", kv.second.launch_phase_contact_time_ms},
                {"dynamics_version", kv.second.dynamics_version}
            };
        }
        return j.dump();
    }
    uint32_t id = std::stoul(it->second);
    auto result = impl_->ballistic->get_latest_result(id);
    json j = {
        {"initial_velocity", result.shot_record.initial_velocity},
        {"range", result.source_data.range},
        {"mach_number", result.mach_number_at_launch},
        {"drag_coefficient", result.drag_coefficient_at_launch},
        {"max_launch_acceleration_g", result.max_launch_acceleration_g},
        {"contact_phase_time_ms", result.launch_phase_contact_time_ms},
        {"dynamics_version", result.dynamics_version}
    };
    return j.dump();
}

static std::map<std::string, std::string> parse_url_params(const std::string& path) {
    std::map<std::string, std::string> params;
    size_t qpos = path.find('?');
    if (qpos == std::string::npos) return params;

    std::string query = path.substr(qpos + 1);
    size_t pos = 0;
    while (pos < query.size()) {
        size_t amp = query.find('&', pos);
        std::string pair = (amp == std::string::npos) ? query.substr(pos) : query.substr(pos, amp - pos);
        size_t eq = pair.find('=');
        if (eq != std::string::npos) {
            std::string key = pair.substr(0, eq);
            std::string val = pair.substr(eq + 1);
            params[key] = val;
        }
        if (amp == std::string::npos) break;
        pos = amp + 1;
    }
    return params;
}

static std::string get_path(const std::string& request) {
    size_t sp1 = request.find(' ');
    if (sp1 == std::string::npos) return "/";
    size_t sp2 = request.find(' ', sp1 + 1);
    if (sp2 == std::string::npos) return "/";
    std::string path = request.substr(sp1 + 1, sp2 - sp1 - 1);
    size_t qpos = path.find('?');
    if (qpos != std::string::npos) return path.substr(0, qpos);
    return path;
}

static std::string build_response(const std::string& body, const std::string& content_type = "application/json") {
    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n";
    oss << "Content-Type: " << content_type << "; charset=utf-8\r\n";
    oss << "Access-Control-Allow-Origin: *\r\n";
    oss << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    oss << "Access-Control-Allow-Headers: Content-Type\r\n";
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << body;
    return oss.str();
}

void HttpServer::register_routes() {}

bool HttpServer::start() {
#ifdef _WIN32
    if (WSAStartup(MAKEWORD(2, 2), &impl_->wsa_data) != 0) {
        std::cerr << "[HTTP] WSAStartup failed" << std::endl;
        return false;
    }
#endif

    impl_->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (impl_->server_fd < 0) {
        std::cerr << "[HTTP] Failed to create socket" << std::endl;
        return false;
    }

    int opt = 1;
#ifdef _WIN32
    setsockopt(impl_->server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(impl_->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(impl_->port);

    if (bind(impl_->server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "[HTTP] Failed to bind port " << impl_->port << std::endl;
        return false;
    }

    if (listen(impl_->server_fd, 10) < 0) {
        std::cerr << "[HTTP] Listen failed" << std::endl;
        return false;
    }

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
#ifdef _WIN32
    setsockopt(impl_->server_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#else
    setsockopt(impl_->server_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    impl_->running = true;
    impl_->server_thread = std::thread([this]() {
        while (impl_->running) {
            sockaddr_in client_addr;
#ifdef _WIN32
            int addr_len = sizeof(client_addr);
            SOCKET client_fd = accept(impl_->server_fd, (sockaddr*)&client_addr, &addr_len);
#else
            socklen_t addr_len = sizeof(client_addr);
            int client_fd = accept(impl_->server_fd, (sockaddr*)&client_addr, &addr_len);
#endif

            if (client_fd < 0) {
                if (impl_->running) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                continue;
            }

            char buffer[8192];
#ifdef _WIN32
            int bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
#else
            ssize_t bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
#endif

            std::string response;
            if (bytes > 0) {
                buffer[bytes] = '\0';
                std::string request(buffer);
                std::string path = get_path(request);
                auto params = parse_url_params(buffer);

                std::string body;
                if (path == "/api/crossbow-types") {
                    body = handle_get_crossbow_types(params);
                } else if (path == "/api/sensor-data") {
                    body = handle_get_sensor_data(params);
                } else if (path == "/api/shot-history") {
                    body = handle_get_shot_history(params);
                } else if (path == "/api/alerts") {
                    body = handle_get_alerts(params);
                } else if (path == "/api/accuracy") {
                    body = handle_get_accuracy(params);
                } else if (path == "/api/simulate-shot") {
                    body = handle_simulate_shot(params);
                } else if (path == "/api/run-analysis") {
                    body = handle_run_accuracy_analysis(params);
                } else if (path == "/api/resolve-alert") {
                    body = handle_resolve_alert(params);
                } else if (path == "/api/system-status") {
                    body = handle_get_system_status(params);
                } else if (path == "/api/ballistic-result") {
                    body = handle_get_ballistic_result(params);
                } else if (path == "/") {
                    body = handle_get_system_status(params);
                } else {
                    body = "{\"status\":\"running\",\"version\":\"2.0\"}";
                }
                response = build_response(body);
            } else {
                response = build_response("{\"error\":\"bad request\"}");
            }

#ifdef _WIN32
            send(client_fd, response.c_str(), response.size(), 0);
            closesocket(client_fd);
#else
            send(client_fd, response.c_str(), response.size(), 0);
            close(client_fd);
#endif
        }
    });

    std::cout << "[HTTP] Server started on port " << impl_->port << std::endl;
    return true;
}

void HttpServer::stop() {
    impl_->running = false;
    if (impl_->server_thread.joinable()) {
        impl_->server_thread.join();
    }
    if (impl_->server_fd >= 0) {
#ifdef _WIN32
        closesocket(impl_->server_fd);
        WSACleanup();
#else
        close(impl_->server_fd);
#endif
        impl_->server_fd = -1;
    }
    std::cout << "[HTTP] Server stopped" << std::endl;
}

bool HttpServer::is_running() const {
    return impl_->running;
}
