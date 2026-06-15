#pragma once

#include "common.h"
#include "message_queue.h"
#include <string>
#include <memory>

class UdpReceiver {
public:
    UdpReceiver(int port, std::shared_ptr<SensorQueue> queue);
    ~UdpReceiver();

    bool start();
    void stop();
    bool is_running() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
