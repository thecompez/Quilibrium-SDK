module;
#include <memory>
#include <string>

export module quilibrium.transport.curl;
import quilibrium.core;
import quilibrium.protocol;

export namespace quilibrium::transport {

struct curl_options final {
    std::string user_agent{"quilibrium-cpp-sdk/1.0"};
    bool verify_tls{true};
    bool follow_redirects{true};
};

/** libcurl-backed HTTP transport. libcurl handles HTTP/1.1, HTTP/2 and TLS. */
[[nodiscard]] http_transport_ptr make_curl_http_transport(curl_options options = {});

/** Unary gRPC transport using standard five-byte gRPC message framing over HTTP/2. */
[[nodiscard]] protocol::transport_ptr make_curl_grpc_transport(
    http_transport_ptr http = {},
    curl_options options = {}
);

} // namespace quilibrium::transport
