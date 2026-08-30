module;
#include <chrono>
#include <string>

export module quilibrium.sigv4;
import quilibrium.core;

export namespace quilibrium::auth {
struct sigv4_credentials final {
    std::string access_key_id{};
    std::string secret_access_key{};
    std::string session_token{};
};
class sigv4_signer final {
public:
    sigv4_signer(sigv4_credentials credentials, std::string region, std::string service);
    [[nodiscard]] status sign(http_request& request, std::chrono::system_clock::time_point now = std::chrono::system_clock::now()) const;
private:
    sigv4_credentials credentials_;
    std::string region_;
    std::string service_;
};
} // namespace quilibrium::auth
