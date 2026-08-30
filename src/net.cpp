module;
#include <expected>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <mutex>
#include <string>
#include <vector>

module quilibrium.net;

namespace quilibrium::net {

endpoint_pool::endpoint_pool(std::vector<endpoint> endpoints) {
    endpoints_.reserve(endpoints.size());
    for (auto& item : endpoints) endpoints_.push_back(endpoint_health{.value=std::move(item)});
}

result<endpoint> endpoint_pool::select() const {
    std::scoped_lock lock{mutex_};
    if (endpoints_.empty()) {
        return std::unexpected(error{.domain=error_domain::configuration, .code=10, .message="endpoint pool is empty"});
    }
    const auto now = std::chrono::steady_clock::now();
    const endpoint_health* best = nullptr;
    double best_score = std::numeric_limits<double>::infinity();
    for (const auto& item : endpoints_) {
        if (item.quarantine_until > now) continue;
        const double latency = item.latency_ewma_ms > 0.0 ? item.latency_ewma_ms : 500.0;
        const double score = latency * (1.0 + static_cast<double>(item.consecutive_failures) * 0.75);
        if (score < best_score) { best = &item; best_score = score; }
    }
    if (!best) {
        best = &*std::min_element(endpoints_.begin(), endpoints_.end(), [](const auto& a, const auto& b) {
            return a.quarantine_until < b.quarantine_until;
        });
    }
    return best->value;
}

void endpoint_pool::report_success(const endpoint& value, std::chrono::milliseconds latency) {
    std::scoped_lock lock{mutex_};
    for (auto& item : endpoints_) {
        if (item.value.host == value.host && item.value.port == value.port) {
            const double sample = static_cast<double>(latency.count());
            item.latency_ewma_ms = item.latency_ewma_ms == 0.0 ? sample : (0.2 * sample + 0.8 * item.latency_ewma_ms);
            item.consecutive_failures = 0;
            item.quarantine_until = {};
            return;
        }
    }
}

void endpoint_pool::report_failure(const endpoint& value) {
    std::scoped_lock lock{mutex_};
    for (auto& item : endpoints_) {
        if (item.value.host == value.host && item.value.port == value.port) {
            ++item.consecutive_failures;
            const auto shift = std::min<std::uint32_t>(item.consecutive_failures, 6U);
            const auto seconds = std::chrono::seconds{1U << shift};
            item.quarantine_until = std::chrono::steady_clock::now() + seconds;
            return;
        }
    }
}

std::vector<endpoint_health> endpoint_pool::snapshot() const {
    std::scoped_lock lock{mutex_};
    return endpoints_;
}

} // namespace quilibrium::net
