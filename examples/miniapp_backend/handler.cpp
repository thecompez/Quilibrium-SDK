#include <cstdint>
#include <string>

import quilibrium;

struct HttpReply final {
    int status{};
    std::string content_type{};
    std::string body{};
};

HttpReply get_user_for_miniapp(quilibrium::sdk& sdk, std::uint64_t fid) {
    auto response = quilibrium::sync_wait(sdk.hypersnap().get(
        "/v2/farcaster/user",
        {{"fid", std::to_string(fid)}}
    ));

    if (!response) {
        return {502, "application/json", R"({"error":"upstream unavailable"})"};
    }

    return {
        response->status_code,
        "application/json",
        std::string(reinterpret_cast<const char*>(response->body.data()), response->body.size())
    };
}
