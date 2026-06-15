#include "udp_receiver.h"
#include "logger.h"
#include "metrics.h"
#include <iostream>
#include <cstring>
#include <thread>
#include <atomic>
#include <sstream>
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

struct UdpReceiver::Impl {
    int port;
    std::shared_ptr<SensorQueue> output_queue;
    std::atomic<bool> running;
    std::thread receiver_thread;
    uint64_t received_count;
    uint64_t parse_error_count;
    uint64_t queue_full_count;
#ifdef _WIN32
    SOCKET sock_fd;
    WSADATA wsa_data;
#else
    int sock_fd;
#endif

    Impl(int p, std::shared_ptr<SensorQueue> q)
        : port(p), output_queue(q), running(false),
          received_count(0), parse_error_count(0), queue_full_count(0),
          sock_fd(-1) {}

    bool validate_sensor_data(const SensorData& data) {
        if (data.crossbow_id == 0) return false;
        if (data.arrow_velocity <= 0 || data.arrow_velocity > 500) return false;
        if (data.range < 0 || data.range > 5000) return false;
        if (data.bow_string_tension < 0) return false;
        if (data.bow_arm_deformation < 0) return false;
        if (data.aim_angle < -90 || data.aim_angle > 90) return false;
        if (data.temperature < -50 || data.temperature > 150) return false;
        if (data.wind_speed < 0 || data.wind_speed > 100) return false;
        return true;
    }
};

UdpReceiver::UdpReceiver(int port, std::shared_ptr<SensorQueue> queue)
    : impl_(std::make_unique<Impl>(port, queue)) {}

UdpReceiver::~UdpReceiver() {
    stop();
}

bool UdpReceiver::start() {
#ifdef _WIN32
    if (WSAStartup(MAKEWORD(2, 2), &impl_->wsa_data) != 0) {
        LOG_ERROR("WSAStartup failed");
        return false;
    }
#endif

    impl_->sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (impl_->sock_fd < 0) {
        LOG_ERROR("Failed to create socket");
        return false;
    }

    int buf_size = 1024 * 1024;
#ifdef _WIN32
    setsockopt(impl_->sock_fd, SOL_SOCKET, SO_RCVBUF, (const char*)&buf_size, sizeof(buf_size));
#else
    setsockopt(impl_->sock_fd, SOL_SOCKET, SO_RCVBUF, &buf_size, sizeof(buf_size));
#endif

    sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(impl_->port);

    if (bind(impl_->sock_fd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        LOG_ERROR("Failed to bind to port {}", impl_->port);
#ifdef _WIN32
        closesocket(impl_->sock_fd);
#else
        close(impl_->sock_fd);
#endif
        return false;
    }

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
#ifdef _WIN32
    setsockopt(impl_->sock_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#else
    setsockopt(impl_->sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    impl_->running = true;
    impl_->receiver_thread = std::thread([this]() {
        char buffer[4096];
        sockaddr_in client_addr;
#ifdef _WIN32
        int addr_len = sizeof(client_addr);
#else
        socklen_t addr_len = sizeof(client_addr);
#endif

        while (impl_->running) {
#ifdef _WIN32
            int bytes_received = recvfrom(impl_->sock_fd, buffer, sizeof(buffer) - 1, 0,
                                          (sockaddr*)&client_addr, &addr_len);
#else
            ssize_t bytes_received = recvfrom(impl_->sock_fd, buffer, sizeof(buffer) - 1, 0,
                                              (sockaddr*)&client_addr, &addr_len);
#endif

            if (bytes_received > 0) {
                buffer[bytes_received] = '\0';
                try {
                    json j = json::parse(buffer);

                    SensorData data;
                    data.timestamp = std::chrono::system_clock::now();
                    data.crossbow_id = j.value("crossbow_id", 0);
                    data.crossbow_name = j.value("crossbow_name", "");
                    data.bow_string_tension = j.value("bow_string_tension", 0.0);
                    data.bow_arm_deformation = j.value("bow_arm_deformation", 0.0);
                    data.arrow_velocity = j.value("arrow_velocity", 0.0);
                    data.range = j.value("range", 0.0);
                    data.spread_x = j.value("spread_x", 0.0);
                    data.spread_y = j.value("spread_y", 0.0);
                    data.aim_angle = j.value("aim_angle", 0.0);
                    data.temperature = j.value("temperature", 25.0);
                    data.humidity = j.value("humidity", 50.0);
                    data.wind_speed = j.value("wind_speed", 0.0);
                    data.wind_direction = j.value("wind_direction", 0.0);

                    if (!impl_->validate_sensor_data(data)) {
                        impl_->parse_error_count++;
                        LOG_ERROR("Data validation failed, crossbow_id={}", data.crossbow_id);
                        METRICS_UDP_ERROR();
                        continue;
                    }

                    if (!impl_->output_queue->push(data)) {
                        impl_->queue_full_count++;
                        if (impl_->queue_full_count % 100 == 0) {
                            LOG_WARN("Queue full, dropped {} msgs", impl_->queue_full_count);
                        }
                    } else {
                        impl_->received_count++;
                        METRICS_UDP_RECEIVED();
                        if (impl_->received_count % 100 == 0) {
                            LOG_INFO("Received {} msgs, queue_size={}", impl_->received_count, impl_->output_queue->size());
                        }
                    }
                } catch (const std::exception& e) {
                    impl_->parse_error_count++;
                    LOG_ERROR("Parse error ({}): {}", impl_->parse_error_count, e.what());
                    METRICS_UDP_ERROR();
                }
            }
        }
    });

    LOG_INFO("Receiver started on port {}", impl_->port);
    return true;
}

void UdpReceiver::stop() {
    impl_->running = false;
    if (impl_->output_queue) {
        impl_->output_queue->stop();
    }
    if (impl_->receiver_thread.joinable()) {
        impl_->receiver_thread.join();
    }
    if (impl_->sock_fd >= 0) {
#ifdef _WIN32
        closesocket(impl_->sock_fd);
        WSACleanup();
#else
        close(impl_->sock_fd);
#endif
        impl_->sock_fd = -1;
    }
    LOG_INFO("Receiver stopped. Total: received={}, parse_errors={}, queue_dropped={}", impl_->received_count, impl_->parse_error_count, impl_->queue_full_count);
}

bool UdpReceiver::is_running() const {
    return impl_->running;
}
