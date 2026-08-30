module;
#include <expected>
#include <charconv>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <memory>
#include <coroutine>
#include <utility>

module quilibrium.core;

namespace quilibrium {

http_transport::http_transport(std::shared_ptr<void> context, send_function function)
    : context_(std::move(context)), function_(function) {}

result<http_response> http_transport::send_now(http_request value, call_options options) {
    if (!function_) {
        return std::unexpected(error{.domain=error_domain::configuration,.code=8,.message="HTTP transport callback is not configured"});
    }
    return function_(context_.get(), std::move(value), options);
}

task<result<http_response>> http_transport::send(http_request value, call_options options) {
    co_return send_now(std::move(value), options);
}

http_transport::operator bool() const noexcept { return function_ != nullptr; }

http_transport_ptr make_http_transport(std::shared_ptr<void> context, http_transport::send_function function) {
    return std::make_shared<http_transport>(std::move(context), function);
}

std::string endpoint::authority() const {
    const bool default_port = (scheme == "https" && port == 443) || (scheme == "http" && port == 80);
    return default_port ? host : host + ":" + std::to_string(port);
}

std::string endpoint::origin() const { return scheme + "://" + authority(); }

result<endpoint> parse_endpoint(std::string_view url) {
    const auto scheme_end = url.find("://");
    if (scheme_end == std::string_view::npos) {
        return std::unexpected(error{.domain=error_domain::validation,.code=4,.message="endpoint URL must include a scheme"});
    }
    endpoint out{};
    out.scheme = std::string(url.substr(0, scheme_end));
    if (out.scheme != "http" && out.scheme != "https") {
        return std::unexpected(error{.domain=error_domain::validation,.code=5,.message="only http and https endpoints are supported"});
    }
    auto rest = url.substr(scheme_end + 3);
    const auto slash = rest.find('/');
    auto authority = slash == std::string_view::npos ? rest : rest.substr(0, slash);
    out.base_path = slash == std::string_view::npos ? std::string{} : std::string(rest.substr(slash));
    if (authority.empty()) {
        return std::unexpected(error{.domain=error_domain::validation,.code=6,.message="endpoint host is empty"});
    }
    const auto colon = authority.rfind(':');
    if (colon != std::string_view::npos && authority.find(':') == colon) {
        out.host = std::string(authority.substr(0, colon));
        unsigned long port_value = 0;
        const auto port_text = authority.substr(colon + 1);
        const auto* begin = port_text.data();
        const auto* end = begin + port_text.size();
        auto parsed = std::from_chars(begin, end, port_value);
        if (parsed.ec != std::errc{} || parsed.ptr != end || port_value > std::numeric_limits<std::uint16_t>::max()) {
            return std::unexpected(error{.domain=error_domain::validation,.code=7,.message="invalid endpoint port"});
        }
        out.port = static_cast<std::uint16_t>(port_value);
    } else {
        out.host = std::string(authority);
        out.port = out.scheme == "https" ? 443 : 80;
    }
    return out;
}

std::string hex(byte_view data) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string out(data.size() * 2, '\0');
    for (std::size_t i = 0; i < data.size(); ++i) {
        const auto value = std::to_integer<unsigned int>(data[i]);
        out[i * 2] = digits[(value >> 4U) & 0xFU];
        out[i * 2 + 1] = digits[value & 0xFU];
    }
    return out;
}

result<bytes> unhex(std::string_view text) {
    if ((text.size() % 2U) != 0U) {
        return std::unexpected(error{.domain=error_domain::validation, .code=2, .message="hex string must have even length"});
    }
    bytes out(text.size() / 2U);
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (std::size_t i = 0; i < out.size(); ++i) {
        const int hi = nibble(text[i * 2]);
        const int lo = nibble(text[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            return std::unexpected(error{.domain=error_domain::validation, .code=3, .message="invalid hex digit"});
        }
        out[i] = static_cast<byte>((hi << 4) | lo);
    }
    return out;
}

byte_view as_bytes(std::string_view text) noexcept {
    return {reinterpret_cast<const byte*>(text.data()), text.size()};
}

std::string as_string(byte_view data) {
    return {reinterpret_cast<const char*>(data.data()), data.size()};
}

std::string percent_encode(std::string_view text, bool preserve_slash) {
    static constexpr char digits[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(text.size());
    for (const char ch : text) {
        const auto c = static_cast<unsigned char>(ch);
        const bool unreserved = std::isalnum(c) != 0 || c == '-' || c == '_' || c == '.' || c == '~' || (preserve_slash && c == '/');
        if (unreserved) {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(digits[(c >> 4U) & 0xFU]);
            out.push_back(digits[c & 0xFU]);
        }
    }
    return out;
}

} // namespace quilibrium
