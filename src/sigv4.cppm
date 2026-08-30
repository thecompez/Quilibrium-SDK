module;
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

export module quilibrium.sigv4;
import quilibrium.core;

export namespace quilibrium::auth {

struct sigv4_credentials final {
    std::string access_key_id{};
    std::string secret_access_key{};
    std::string session_token{};
};

/** Stable SigV4-specific error codes exposed through quilibrium::error::code. */
enum class sigv4_error_code : std::int32_t {
    incomplete_credentials = 100,
    invalid_expiration = 101,
    unsupported_endpoint_scheme = 102,
    missing_signed_header = 103,
    conflicting_query_parameter = 104,
    invalid_request_target = 105
};

/** Payload treatment used while constructing a SigV4 presigned request. */
enum class presign_payload_mode : std::uint8_t {
    /** Standard S3 presigning behavior for future GET/HEAD/PUT payloads. */
    unsigned_payload,
    /** Hash request.body and bind that exact payload to the signature. */
    hash_request_body
};

/** Options controlling query-string SigV4 authentication. */
struct presign_options final {
    std::chrono::seconds expires{900};
    /** Additional request headers to bind to the signature. `host` is always signed. */
    std::vector<std::string> signed_headers{};
    presign_payload_mode payload_mode{presign_payload_mode::unsigned_payload};
};

/**
 * A self-contained presigned request.
 *
 * `required_headers` contains caller-supplied headers that are part of the
 * signature and therefore MUST be sent with the exact returned values.
 * The Host header is intentionally omitted because normal HTTP clients derive
 * it from `url` automatically.
 */
struct presigned_request final {
    std::string url{};
    http_headers required_headers{};
    std::chrono::system_clock::time_point expires_at{};
};

class sigv4_signer final {
public:
    sigv4_signer(sigv4_credentials credentials, std::string region, std::string service);

    /** Adds standard SigV4 Authorization headers to a request. */
    [[nodiscard]] status sign(
        http_request& request,
        std::chrono::system_clock::time_point now = std::chrono::system_clock::now()) const;

    /**
     * Produces AWS Signature Version 4 query authentication for a request.
     * Existing query parameters are preserved, canonicalized and sorted.
     */
    [[nodiscard]] result<presigned_request> presign(
        http_request request,
        presign_options options = {},
        std::chrono::system_clock::time_point now = std::chrono::system_clock::now()) const;

private:
    sigv4_credentials credentials_;
    std::string region_;
    std::string service_;
};

} // namespace quilibrium::auth
