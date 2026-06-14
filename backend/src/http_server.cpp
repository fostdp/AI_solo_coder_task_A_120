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
    std::shared_ptr<MqttAlertManager> alert_manager;
    std::atomic<bool> running;
    std::thread server_thread;
    std::mutex data_mutex;
    std::vector<SensorData> recent_sensor_data;
    std::map<uint32_t, std::vector<SensorData>> sensor_data_by_crossbow;
#ifdef _WIN32
    SOCKET server_fd;
    WSADATA wsa_data;
#else
    int server_fd;
#endif

    Impl(int p, std::shared_ptr<ClickHouseStorage> s, std::shared_ptr<MqttAlertManager> a)
        : port(p), storage(s), alert_manager(a), running(false), server_fd(-1) {}

    void add_sensor_data(const SensorData& data) {
        std::lock_guard<std::mutex> lock(data_mutex);
        recent_sensor_data.push_back(data);
        if (recent_sensor_data.size() > 10000) {
            recent_sensor_data.erase(recent_sensor_data.begin());
        }
        sensor_data_by_crossbow[data.crossbow_id].push_back(data);
        if (sensor_data_by_crossbow[data.crossbow_id].size() > 2000) {
            sensor_data_by_crossbow[data.crossbow_id].erase(
                sensor_data_by_crossbow[data.crossbow_id].begin());
        }
    }

    std::vector<SensorData> get_crossbow_data(uint32_t crossbow_id) {
        std::lock_guard<std::mutex> lock(data_mutex);
        auto it = sensor_data_by_crossbow.find(crossbow_id);
        if (it != sensor_data_by_crossbow.end()) {
            return it->second;
        }
        return {};
    }
};

HttpServer::HttpServer(int port,
                       std::shared_ptr<ClickHouseStorage> storage,
                       std::shared_ptr<MqttAlertManager> alert_manager)
    : impl_(std::make_unique<Impl>(port, storage, alert_manager)) {}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::add_sensor_data(const SensorData& data) {
    impl_->add_sensor_data(data);
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
    if (it != params.end()) {
        uint32_t id = std::stoul(it->second);
        auto data = impl_->get_crossbow_data(id);
        auto limit_it = params.find("limit");
        size_t limit = limit_it != params.end() ? std::stoul(limit_it->second) : 100;
        size_t start = data.size() > limit ? data.size() - limit : 0;
        for (size_t i = start; i < data.size(); i++) {
            const auto& d = data[i];
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
    if (it != params.end()) {
        uint32_t id = std::stoul(it->second);
        auto data = impl_->get_crossbow_data(id);
        auto limit_it = params.find("limit");
        size_t limit = limit_it != params.end() ? std::stoul(limit_it->second) : 50;
        size_t start = data.size() > limit ? data.size() - limit : 0;
        for (size_t i = start; i < data.size(); i++) {
            const auto& d = data[i];
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
    auto alerts = impl_->storage->get_active_alerts();
    json j = json::array();
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
    if (it != params.end()) {
        uint32_t id = std::stoul(it->second);
        auto data = impl_->get_crossbow_data(id);

        std::string crossbow_name;
        if (!data.empty()) crossbow_name = data[0].crossbow_name;

        AccuracyAnalyzer analyzer;
        auto analysis = analyzer.analyze(id, crossbow_name, data);

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
    if (it == params.end()) return "{\"error\":\"missing crossbow_id\"}";

    uint32_t id = std::stoul(it->second);
    auto types = impl_->storage->get_crossbow_types();
    CrossbowType crossbow;
    bool found = false;
    for (const auto& t : types) {
        if (t.id == id) {
            crossbow = t;
            found = true;
            break;
        }
    }
    if (!found) return "{\"error\":\"crossbow not found\"}";

    double angle = 15.0;
    double wind_speed = 0.0;
    double wind_direction = 0.0;

    auto angle_it = params.find("angle");
    if (angle_it != params.end()) angle = std::stod(angle_it->second);

    auto wind_it = params.find("wind_speed");
    if (wind_it != params.end()) wind_speed = std::stod(wind_it->second);

    auto wd_it = params.find("wind_direction");
    if (wd_it != params.end()) wind_direction = std::stod(wd_it->second);

    DynamicsModel model(crossbow);
    double initial_velocity = model.calculate_initial_velocity(
        crossbow.draw_weight, crossbow.bow_length * 0.6, crossbow.arrow_mass);

    auto trajectory = model.simulate_trajectory(initial_velocity, angle, wind_speed, wind_direction);
    auto record = model.calculate_shot_results(trajectory);

    json traj = json::array();
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
        {"shot_id", record.shot_id},
        {"initial_velocity", record.initial_velocity},
        {"launch_angle", record.launch_angle},
        {"max_height", record.max_height},
        {"flight_time", record.flight_time},
        {"impact_point", {
            {"x", record.impact_point.x},
            {"y", record.impact_point.y},
            {"z", record.impact_point.z}
        }},
        {"impact_velocity", record.impact_velocity},
        {"kinetic_energy", record.kinetic_energy},
        {"trajectory", traj}
    };
    return j.dump();
}

std::string HttpServer::handle_run_accuracy_analysis(const std::map<std::string, std::string>& params) {
    auto it = params.find("crossbow_id");
    if (it == params.end()) return "{\"error\":\"missing crossbow_id\"}";

    uint32_t id = std::stoul(it->second);
    auto data = impl_->get_crossbow_data(id);

    std::string crossbow_name;
    if (!data.empty()) crossbow_name = data[0].crossbow_name;

    AccuracyAnalyzer analyzer;
    auto analysis = analyzer.analyze(id, crossbow_name, data);
    impl_->storage->insert_accuracy_analysis(analysis);

    return handle_get_accuracy(params);
}

std::string HttpServer::handle_resolve_alert(const std::map<std::string, std::string>& params) {
    return "{\"status\":\"ok\"}";
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
                } else {
                    body = "{\"status\":\"running\",\"version\":\"1.0\"}";
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
