#include "metrics.h"
#include "logger.h"

namespace crossbow {

Metrics& Metrics::instance() {
    static Metrics inst;
    return inst;
}

void Metrics::init(int port, const std::string& bind_addr) {
    if (initialized_) return;

#ifdef HAS_PROMETHEUS
    try {
        std::string bind = bind_addr + ":" + std::to_string(port);
        exposer_ = std::make_unique<prometheus::Exposer>(bind);
        registry_ = std::make_shared<prometheus::Registry>();

        exposer_->RegisterCollectable(registry_);

        // UDP metrics
        udp_family_ = &prometheus::BuildCounter()
            .Name("crossbow_udp_packets_total")
            .Help("Total UDP packets received")
            .Register(*registry_);
        udp_received_counter_ = &udp_family_->Add({{"status", "ok"}});
        udp_errors_counter_ = &udp_family_->Add({{"status", "error"}});

        // Processing metrics
        processing_family_ = &prometheus::BuildCounter()
            .Name("crossbow_processed_total")
            .Help("Total processed sensor data")
            .Register(*registry_);
        ballistic_counter_ = &processing_family_->Add({{"module", "ballistic"}});
        accuracy_counter_ = &processing_family_->Add({{"module", "accuracy"}});

        // Alert metrics
        alert_family_ = &prometheus::BuildCounter()
            .Name("crossbow_alerts_total")
            .Help("Total alerts generated")
            .Register(*registry_);
        alerts_fired_counter_ = &alert_family_->Add({{"type", "fired"}});
        alerts_mqtt_counter_ = &alert_family_->Add({{"type", "mqtt_published"}});

        // Queue gauges
        queue_family_ = &prometheus::BuildGauge()
            .Name("crossbow_queue_size")
            .Help("Current message queue size")
            .Register(*registry_);
        queue_ballistic_gauge_ = &queue_family_->Add({{"queue", "ballistic"}});
        queue_accuracy_gauge_ = &queue_family_->Add({{"queue", "accuracy"}});
        queue_alarm_gauge_ = &queue_family_->Add({{"queue", "alarm"}});

        // Latency histogram
        latency_family_ = &prometheus::BuildHistogram()
            .Name("crossbow_ballistic_latency_ms")
            .Help("Ballistic simulation latency in ms")
            .Register(*registry_);
        ballistic_latency_ = &latency_family_->Add(
            {}, prometheus::Histogram::BucketBoundaries{0.1, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 50.0}
        );

        // HTTP requests
        http_family_ = &prometheus::BuildCounter()
            .Name("crossbow_http_requests_total")
            .Help("Total HTTP requests by endpoint")
            .Register(*registry_);

        initialized_ = true;
        LOG_INFO("Prometheus metrics endpoint started on port {}", port);
    } catch (const std::exception& e) {
        LOG_WARN("Failed to start Prometheus endpoint: {}", e.what());
    }
#else
    (void)port;
    (void)bind_addr;
    LOG_INFO("Metrics disabled (prometheus-cpp not available)");
#endif
}

void Metrics::increment_udp_received() {
    ++udp_received_;
#ifdef HAS_PROMETHEUS
    if (udp_received_counter_) udp_received_counter_->Increment();
#endif
}

void Metrics::increment_udp_errors() {
    ++udp_errors_;
#ifdef HAS_PROMETHEUS
    if (udp_errors_counter_) udp_errors_counter_->Increment();
#endif
}

void Metrics::increment_ballistic_processed() {
    ++ballistic_processed_;
#ifdef HAS_PROMETHEUS
    if (ballistic_counter_) ballistic_counter_->Increment();
#endif
}

void Metrics::increment_accuracy_processed() {
    ++accuracy_processed_;
#ifdef HAS_PROMETHEUS
    if (accuracy_counter_) accuracy_counter_->Increment();
#endif
}

void Metrics::increment_alerts_fired() {
    ++alerts_fired_;
#ifdef HAS_PROMETHEUS
    if (alerts_fired_counter_) alerts_fired_counter_->Increment();
#endif
}

void Metrics::increment_alerts_mqtt_published() {
    ++alerts_mqtt_published_;
#ifdef HAS_PROMETHEUS
    if (alerts_mqtt_counter_) alerts_mqtt_counter_->Increment();
#endif
}

void Metrics::increment_http_requests(const std::string& endpoint) {
    std::lock_guard<std::mutex> lock(http_mutex_);
    http_requests_[endpoint]++;
#ifdef HAS_PROMETHEUS
    if (http_family_) {
        auto& counter = http_family_->Add({{"endpoint", endpoint}});
        counter.Increment();
    }
#endif
}

void Metrics::set_queue_size_ballistic(size_t size) {
    queue_ballistic_ = size;
#ifdef HAS_PROMETHEUS
    if (queue_ballistic_gauge_) queue_ballistic_gauge_->Set(static_cast<double>(size));
#endif
}

void Metrics::set_queue_size_accuracy(size_t size) {
    queue_accuracy_ = size;
#ifdef HAS_PROMETHEUS
    if (queue_accuracy_gauge_) queue_accuracy_gauge_->Set(static_cast<double>(size));
#endif
}

void Metrics::set_queue_size_alarm(size_t size) {
    queue_alarm_ = size;
#ifdef HAS_PROMETHEUS
    if (queue_alarm_gauge_) queue_alarm_gauge_->Set(static_cast<double>(size));
#endif
}

void Metrics::observe_ballistic_latency_ms(double ms) {
#ifdef HAS_PROMETHEUS
    if (ballistic_latency_) ballistic_latency_->Observe(ms);
#else
    (void)ms;
#endif
}

}
