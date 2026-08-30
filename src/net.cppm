module;
#include <chrono>
#include <cstdint>
#include <mutex>
#include <vector>

export module quilibrium.net;
import quilibrium.core;

export namespace quilibrium::net {

/** Health information used by the endpoint selector. */
struct endpoint_health final {
    endpoint value{};
    double latency_ewma_ms{0.0};
    std::uint32_t consecutive_failures{0};
    std::chrono::steady_clock::time_point quarantine_until{};
};

/** Thread-safe latency-aware endpoint pool with temporary quarantine. */
class endpoint_pool final {
public:
    explicit endpoint_pool(std::vector<endpoint> endpoints);
    [[nodiscard]] result<endpoint> select() const;
    void report_success(const endpoint& value, std::chrono::milliseconds latency);
    void report_failure(const endpoint& value);
    [[nodiscard]] std::vector<endpoint_health> snapshot() const;
private:
    mutable std::mutex mutex_;
    std::vector<endpoint_health> endpoints_;
};

} // namespace quilibrium::net
