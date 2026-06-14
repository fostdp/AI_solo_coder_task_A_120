#pragma once

#include "common.h"
#include <string>
#include <functional>
#include <memory>

class UdpReceiver {
public:
    using DataCallback = std::function<void(const SensorData&)>;

    UdpReceiver(int port, DataCallback callback);
    ~UdpReceiver();

    bool start();
    void stop();
    bool is_running() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
