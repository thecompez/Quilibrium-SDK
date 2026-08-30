module;
#include <array>
#include <algorithm>
#include <chrono>
#include <concepts>
#include <coroutine>
#include <exception>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

export module quilibrium.core;

export namespace quilibrium {

using byte = std::byte;
using bytes = std::vector<byte>;
using byte_view = std::span<const byte>;

enum class error_domain : std::uint8_t {
    configuration,
    validation,
    transport,
    authentication,
    protocol,
    service,
    cryptography,
    compatibility,
    cancelled,
    internal
};

/** Describes a stable SDK error independent from a transport implementation. */
struct error final {
    error_domain domain{error_domain::internal};
    std::int32_t code{};
    std::string message{};
    std::optional<std::int32_t> http_status{};
    std::optional<std::string> upstream_code{};
    bool retryable{false};
};

template <typename T>
using result = std::expected<T, error>;

using status = result<void>;

/**
 * Small scheduler-neutral eager coroutine task.
 *
 * Execution starts immediately until the first real suspension point. This makes
 * temporary service facades safe while preserving normal co_await semantics for
 * genuinely asynchronous extension transports. get() is safe for the SDK's
 * default blocking transports, CLI, Qt worker threads and language bindings.
 */
template <typename T>
class [[nodiscard]] task final {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type final {
        std::optional<T> value{};
        std::exception_ptr exception{};
        std::coroutine_handle<> continuation{};

        [[nodiscard]] task get_return_object() noexcept { return task{handle_type::from_promise(*this)}; }
        [[nodiscard]] constexpr std::suspend_never initial_suspend() const noexcept { return {}; }

        struct final_awaiter final {
            [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }
            std::coroutine_handle<> await_suspend(handle_type handle) const noexcept {
                return handle.promise().continuation ? handle.promise().continuation : std::noop_coroutine();
            }
            constexpr void await_resume() const noexcept {}
        };

        [[nodiscard]] constexpr final_awaiter final_suspend() const noexcept { return {}; }
        void unhandled_exception() noexcept { exception = std::current_exception(); }

        template <typename U>
        requires std::constructible_from<T, U&&>
        void return_value(U&& result_value) { value.emplace(std::forward<U>(result_value)); }
    };

    task() = default;
    task(const task&) = delete;
    task& operator=(const task&) = delete;
    task(task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    task& operator=(task&& other) noexcept {
        if (this != &other) {
            if (handle_) handle_.destroy();
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }
    ~task() { if (handle_) handle_.destroy(); }

    [[nodiscard]] bool await_ready() const noexcept { return !handle_ || handle_.done(); }
    void await_suspend(std::coroutine_handle<> continuation) noexcept {
        handle_.promise().continuation = continuation;
        handle_.resume();
    }
    T await_resume() {
        auto& promise = handle_.promise();
        if (promise.exception) std::rethrow_exception(promise.exception);
        return std::move(*promise.value);
    }

    /** Runs a synchronously-completing task to completion. */
    T get() {
        if (!handle_) throw std::logic_error("attempted to run an empty task");
        while (!handle_.done()) handle_.resume();
        return await_resume();
    }

private:
    explicit task(handle_type handle) noexcept : handle_(handle) {}
    handle_type handle_{};
};

template <typename T>
[[nodiscard]] T sync_wait(task<T> value) { return value.get(); }

/** Per-call deadline, cancellation-independent retry and routing options. */
struct call_options final {
    std::chrono::milliseconds timeout{15'000};
    std::uint32_t max_attempts{3};
    bool allow_failover{true};
    bool idempotent{true};
};

/** Network endpoint used by hosted services and protocol RPC transports. */
struct endpoint final {
    std::string scheme{"https"};
    std::string host{};
    std::uint16_t port{443};
    std::string base_path{};

    [[nodiscard]] std::string authority() const;
    [[nodiscard]] std::string origin() const;
    [[nodiscard]] bool operator==(const endpoint&) const noexcept = default;
};

/** Parses an http/https origin with an optional base path. */
[[nodiscard]] result<endpoint> parse_endpoint(std::string_view url);

enum class http_method : std::uint8_t { get, head, post, put, del, patch };
using http_headers = std::map<std::string, std::string, std::less<>>;

/** Transport-neutral HTTP request. */
struct http_request final {
    http_method verb{http_method::get};
    endpoint target_endpoint{};
    std::string target{"/"};
    http_headers header_fields{};
    bytes body{};
};

/** Transport-neutral HTTP response. */
struct http_response final {
    std::int32_t status_code{};
    http_headers header_fields{};
    bytes body{};
};

/**
 * ABI-stable transport bridge. The callback itself is synchronous; the SDK wraps
 * it in task<> so coroutine state never crosses a module/plugin boundary.
 */
class http_transport final {
public:
    using send_function = result<http_response> (*)(void* context, http_request value, call_options options);

    http_transport(std::shared_ptr<void> context, send_function function);
    /** Executes a transport request synchronously. Intended for SDK internals and FFI. */
    [[nodiscard]] result<http_response> send_now(http_request value, call_options options = {});
    /** Coroutine wrapper around send_now(). */
    [[nodiscard]] task<result<http_response>> send(http_request value, call_options options = {});
    [[nodiscard]] explicit operator bool() const noexcept;

private:
    std::shared_ptr<void> context_{};
    send_function function_{};
};
using http_transport_ptr = std::shared_ptr<http_transport>;

[[nodiscard]] http_transport_ptr make_http_transport(std::shared_ptr<void> context, http_transport::send_function function);

/** Converts an HTTP method enum to its canonical uppercase token. */
[[nodiscard]] constexpr std::string_view http_method_name(http_method value) noexcept {
    switch (value) {
        case http_method::get: return "GET";
        case http_method::head: return "HEAD";
        case http_method::post: return "POST";
        case http_method::put: return "PUT";
        case http_method::del: return "DELETE";
        case http_method::patch: return "PATCH";
    }
    return "GET";
}

[[nodiscard]] constexpr bool http_success(std::int32_t status_code) noexcept {
    return status_code >= 200 && status_code < 300;
}

[[nodiscard]] constexpr bool http_retryable(std::int32_t status_code) noexcept {
    return status_code == 408 || status_code == 425 || status_code == 429 || status_code >= 500;
}

template <std::size_t N>
class fixed_bytes final {
public:
    using storage_type = std::array<byte, N>;

    constexpr fixed_bytes() = default;
    explicit constexpr fixed_bytes(storage_type value) noexcept : value_(std::move(value)) {}

    /** Creates a fixed-size value only when the input size is exact. */
    [[nodiscard]] static result<fixed_bytes> from(byte_view input) {
        if (input.size() != N) {
            return std::unexpected(error{
                .domain = error_domain::validation,
                .code = 1,
                .message = "invalid fixed byte length",
                .retryable = false
            });
        }
        storage_type out{};
        std::copy(input.begin(), input.end(), out.begin());
        return fixed_bytes{out};
    }

    [[nodiscard]] constexpr byte_view view() const noexcept { return value_; }
    [[nodiscard]] constexpr const storage_type& storage() const noexcept { return value_; }
    [[nodiscard]] constexpr bool operator==(const fixed_bytes&) const noexcept = default;

private:
    storage_type value_{};
};

using address32 = fixed_bytes<32>;
using address64 = fixed_bytes<64>;
using ed448_public_key = fixed_bytes<57>;
using x448_public_key = fixed_bytes<57>;
using decaf448_public_key = fixed_bytes<56>;
using ed448_signature = fixed_bytes<114>;
using decaf448_signature = fixed_bytes<112>;
using bls48581_public_key = fixed_bytes<585>;
using bls48581_signature = fixed_bytes<74>;

/** Returns a lowercase hexadecimal representation without a prefix. */
[[nodiscard]] std::string hex(byte_view data);

/** Decodes an even-length hexadecimal string. */
[[nodiscard]] result<bytes> unhex(std::string_view text);

/** Returns a byte view over a string without copying. */
[[nodiscard]] byte_view as_bytes(std::string_view text) noexcept;

/** Copies bytes to a UTF-8 string without validating encoding. */
[[nodiscard]] std::string as_string(byte_view data);

/** RFC 3986 percent-encoding suitable for query values and path segments. */
[[nodiscard]] std::string percent_encode(std::string_view text, bool preserve_slash = false);

} // namespace quilibrium
