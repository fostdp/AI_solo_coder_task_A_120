#pragma once

#include <string>
#include <memory>
#include <atomic>
#include <mutex>
#include <unordered_map>

#ifdef HAS_PROMETHEUS
#include "prometheus/exposer.h"
#include "prometheus/registry.h"
#include "prometheus/counter.h"
#include "prometheus/gauge.h"
#include "prometheus/histogram.h"
#include "prometheus/family.h"
#endif

namespace crossbow {

class Metrics {
public:
    static Metrics& instance();

    void init(int port = 9090, const std::string& bind_addr = "0.0.0.0");

    void increment_udp_received();
    void increment_udp_errors();
    void increment_ballistic_processed();
    void increment_accuracy_processed();
    void increment_alerts_fired();
    void increment_alerts_mqtt_published();
    void increment_http_requests(const std::string& endpoint);

    void set_queue_size_ballistic(size_t size);
    void set_queue_size_accuracy(size_t size);
    void set_queue_size_alarm(size_t size);

    void observe_ballistic_latency_ms(double ms);
    void observe_udp_packet_size(size_t bytes);

private:
    Metrics() = default;
    ~Metrics() = default;
    Metrics(const Metrics&) = delete;
    Metrics& operator=(const Metrics&) = delete;

    bool initialized_ = false;

    // Atomic counters for mock mode
    std::atomic<uint64_t> udp_received_{0};
    std::atomic<uint64_t> udp_errors_{0};
    std::atomic<uint64_t> ballistic_processed_{0};
    std::atomic<uint64_t> accuracy_processed_{0};
    std::atomic<uint64_t> alerts_fired_{0};
    std::atomic<uint64_t> alerts_mqtt_published_{0};
    std::atomic<size_t> queue_ballistic_{0};
    std::atomic<size_t> queue_accuracy_{0};
    std::atomic<size_t> queue_alarm_{0};

    std::mutex http_mutex_;
    std::unordered_map<std::string, uint64_t> http_requests_;

#ifdef HAS_PROMETHEUS
    std::unique_ptr<prometheus::Exposer> exposer_;
    std::shared_ptr<prometheus::Registry> registry_;

    prometheus::Family<prometheus::Counter>* udp_family_ = nullptr;
    prometheus::Counter* udp_received_counter_ = nullptr;
    prometheus::Counter* udp_errors_counter_ = nullptr;

    prometheus::Family<prometheus::Counter>* processing_family_ = nullptr;
    prometheus::Counter* ballistic_counter_ = nullptr;
    prometheus::Counter* accuracy_counter_ = nullptr;

    prometheus::Family<prometheus::Counter>* alert_family_ = nullptr;
    prometheus::Counter* alerts_fired_counter_ = nullptr;
    prometheus::Counter* alerts_mqtt_counter_ = nullptr;

    prometheus::Family<prometheus::Gauge>* queue_family_ = nullptr;
    prometheus::Gauge* queue_ballistic_gauge_ = nullptr;
    prometheus::Gauge* queue_accuracy_gauge_ = nullptr;
    prometheus::Gauge* queue_alarm_gauge_ = nullptr;

    prometheus::Family<prometheus::Histogram>* latency_family_ = nullptr;
    prometheus::Histogram* ballistic_latency_ = nullptr;

    prometheus::Family<prometheus::Counter>* http_family_ = nullptr;
#endif
};

#define METRICS_UDP_RECEIVED()      crossbow::Metrics::instance().increment_udp_received()
#define METRICS_UDP_ERROR()         crossbow::Metrics::instance().increment_udp_errors()
#define METRICS_BALLISTIC_DONE()    crossbow::Metrics::instance().increment_ballistic_processed()
#define METRICS_ACCURACY_DONE()     crossbow::Metrics::instance().increment_accuracy_processed()
#define METRICS_ALERT_FIRED()       crossbow::Metrics::instance().increment_alerts_fired()
#define METRICS_MQTT_PUBLISHED()    crossbow::Metrics::instance().increment_alerts_mqtt_published()
#define METRICS_HTTP_REQ(endpoint)  crossbow::Metrics::instance().increment_http_requests(endpoint)
#define METRICS_QUEUE_BALLISTIC(s)  crossbow::Metrics::instance().set_queue_size_ballistic(s)
#define METRICS_QUEUE_ACCURACY(s)   crossbow::Metrics::instance().set_queue_size_accuracy(s)
#define METRICS_QUEUE_ALARM(s)      crossbow::Metrics::instance().set_queue_size_alarm(s)
#define METRICS_BALLISTIC_LATENCY(ms) crossbow::Metrics::instance().observe_ballistic_latency_ms(ms)

}
