#include "udp_receiver.h"
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
    DataCallback callback;
    std::atomic<bool> running;
    std::thread receiver_thread;
#ifdef _WIN32
    SOCKET sock_fd;
    WSADATA wsa_data;
#else
    int sock_fd;
#endif

    Impl(int p, DataCallback cb) : port(p), callback(cb), running(false), sock_fd(-1) {}
};

UdpReceiver::UdpReceiver(int port, DataCallback callback)
    : impl_(std::make_unique<Impl>(port, callback)) {}

UdpReceiver::~UdpReceiver() {
    stop();
}

bool UdpReceiver::start() {
#ifdef _WIN32
    if (WSAStartup(MAKEWORD(2, 2), &impl_->wsa_data) != 0) {
        std::cerr << "[UDP] WSAStartup failed" << std::endl;
        return false;
    }
#endif

    impl_->sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (impl_->sock_fd < 0) {
        std::cerr << "[UDP] Failed to create socket" << std::endl;
        return false;
    }

    sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(impl_->port);

    if (bind(impl_->sock_fd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "[UDP] Failed to bind to port " << impl_->port << std::endl;
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

                    if (impl_->callback) {
                        impl_->callback(data);
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[UDP] Parse error: " << e.what() << std::endl;
                }
            }
        }
    });

    std::cout << "[UDP] Receiver started on port " << impl_->port << std::endl;
    return true;
}

void UdpReceiver::stop() {
    impl_->running = false;
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
    std::cout << "[UDP] Receiver stopped" << std::endl;
}

bool UdpReceiver::is_running() const {
    return impl_->running;
}
