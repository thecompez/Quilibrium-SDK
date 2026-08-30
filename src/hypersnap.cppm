module;
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module quilibrium.hypersnap;
import quilibrium.core;

export namespace quilibrium::hypersnap {

struct config final { std::vector<endpoint> endpoints{}; };
struct response final { std::int32_t status_code{}; http_headers headers{}; bytes json{}; };
using query = std::vector<std::pair<std::string,std::string>>;

/** Low-level complete HyperSnap HTTP client. */
class client final {
public:
    client(config configuration,http_transport_ptr transport);
    ~client();
    client(client&&) noexcept;
    client& operator=(client&&) noexcept;
    client(const client&)=delete;
    client& operator=(const client&)=delete;

    [[nodiscard]] task<result<response>> get(std::string path,query parameters={},call_options options={});
    [[nodiscard]] task<result<response>> post(std::string path,bytes json_payload,http_headers headers={},call_options options={});

    [[nodiscard]] task<result<response>> user_by_fid(std::uint64_t fid,call_options options={});
    [[nodiscard]] task<result<response>> user_by_username(std::string username,call_options options={});
    [[nodiscard]] task<result<response>> search_users(std::string text,std::uint32_t limit=10,call_options options={});
    [[nodiscard]] task<result<response>> cast(std::string identifier,call_options options={});
    [[nodiscard]] task<result<response>> cast_conversation(std::string identifier,std::uint32_t reply_depth=2,call_options options={});
    [[nodiscard]] task<result<response>> search_casts(std::string text,std::uint32_t limit=10,call_options options={});
    [[nodiscard]] task<result<response>> feed(query parameters={},call_options options={});
    [[nodiscard]] task<result<response>> following_feed(std::uint64_t fid,std::uint32_t limit=10,std::string cursor={},call_options options={});
    [[nodiscard]] task<result<response>> trending_feed(std::uint32_t limit=10,std::string cursor={},call_options options={});
    [[nodiscard]] task<result<response>> user_casts(std::uint64_t fid,std::uint32_t limit=10,std::string cursor={},call_options options={});

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

/** Verifies HyperSnap webhook HMAC-SHA512 signatures in constant time. */
[[nodiscard]] result<bool> verify_webhook_signature(std::string_view secret,byte_view payload,std::string_view hexadecimal_signature);

} // namespace quilibrium::hypersnap
