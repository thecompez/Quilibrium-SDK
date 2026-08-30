module;
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

export module quilibrium.qstorage;
import quilibrium.core;

export namespace quilibrium::qstorage {
struct credentials final { std::string access_key_id{}; std::string secret_access_key{}; std::string session_token{}; };
struct config final { std::vector<endpoint> endpoints{}; credentials auth{}; std::string region{"q"}; };
struct object final { std::string key{}; bytes data{}; std::string content_type{"application/octet-stream"}; };
struct raw_response final { std::int32_t status_code{}; http_headers headers{}; bytes body{}; };
class client final {
public:
    client(config configuration, http_transport_ptr transport);
    ~client();
    client(client&&) noexcept;
    client& operator=(client&&) noexcept;
    client(const client&) = delete;
    client& operator=(const client&) = delete;
    [[nodiscard]] task<result<raw_response>> create_bucket(std::string bucket, call_options options = {});
    [[nodiscard]] task<result<raw_response>> delete_bucket(std::string bucket, call_options options = {});
    [[nodiscard]] task<result<raw_response>> list_buckets(call_options options = {});
    [[nodiscard]] task<result<raw_response>> head_bucket(std::string bucket, call_options options = {});
    [[nodiscard]] task<result<raw_response>> put_object(std::string bucket, object value, call_options options = {});
    [[nodiscard]] task<result<raw_response>> get_object(std::string bucket, std::string key, call_options options = {});
    [[nodiscard]] task<result<raw_response>> delete_object(std::string bucket, std::string key, call_options options = {});
    [[nodiscard]] task<result<raw_response>> head_object(std::string bucket, std::string key, call_options options = {});
    [[nodiscard]] task<result<raw_response>> copy_object(std::string bucket, std::string key, std::string copy_source, call_options options = {});
    [[nodiscard]] task<result<raw_response>> list_objects(std::string bucket, std::string query = {}, call_options options = {});
    [[nodiscard]] task<result<raw_response>> create_multipart_upload(std::string bucket, std::string key, std::string content_type = "application/octet-stream", call_options options = {});
    [[nodiscard]] task<result<raw_response>> upload_part(std::string bucket, std::string key, std::string upload_id, std::uint32_t part_number, bytes data, call_options options = {});
    [[nodiscard]] task<result<raw_response>> complete_multipart_upload(std::string bucket, std::string key, std::string upload_id, bytes completion_xml, call_options options = {});
    [[nodiscard]] task<result<raw_response>> abort_multipart_upload(std::string bucket, std::string key, std::string upload_id, call_options options = {});
    [[nodiscard]] task<result<raw_response>> list_parts(std::string bucket, std::string key, std::string upload_id, call_options options = {});
    [[nodiscard]] task<result<raw_response>> list_multipart_uploads(std::string bucket, call_options options = {});
    [[nodiscard]] task<result<raw_response>> execute(http_method verb, std::string target, http_headers headers = {}, bytes body = {}, call_options options = {});
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};
} // namespace quilibrium::qstorage
