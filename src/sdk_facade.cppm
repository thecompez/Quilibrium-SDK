module;
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

export module quilibrium.sdk;
import quilibrium.core;
import quilibrium.json;

export namespace quilibrium {

enum class network : std::uint8_t { mainnet, testnet, custom };
enum class native_service : std::uint8_t { node, connectivity, global, app_shard, hypergraph_comparison, key_registry, dispatch, mixnet, onion, pubsub_proxy, data_ipc, ferret_proxy };

struct sdk_credentials final {
    std::string access_key_id{};
    std::string secret_access_key{};
    std::string session_token{};
};

struct sdk_config final {
    network target_network{network::mainnet};
    std::vector<endpoint> hypersnap_endpoints{};
    std::vector<endpoint> qstorage_endpoints{};
    std::vector<endpoint> qkms_endpoints{};
    std::vector<endpoint> protocol_endpoints{};
    std::optional<sdk_credentials> qstorage_credentials{};
    std::optional<sdk_credentials> qkms_credentials{};
    http_transport_ptr http{};
    bool verify_tls{true};
    std::string user_agent{"quilibrium-cpp-sdk/1.0"};
};

struct service_response final {
    std::int32_t status_code{};
    http_headers headers{};
    bytes body{};
};

struct farcaster_user final {
    std::uint64_t fid{};
    std::string username{};
    std::string display_name{};
    std::string pfp_url{};
    std::string bio{};
    std::uint64_t follower_count{};
    std::uint64_t following_count{};
    std::vector<std::string> verified_addresses{};
    json::value raw{};
};

struct farcaster_cast final {
    std::string hash{};
    farcaster_user author{};
    std::string text{};
    std::string timestamp{};
    std::string parent_hash{};
    std::string parent_url{};
    std::string root_parent_url{};
    std::uint64_t likes{};
    std::uint64_t recasts{};
    std::uint64_t replies{};
    json::value raw{};
};

struct feed_page final {
    std::vector<farcaster_cast> casts{};
    std::string cursor{};
    json::value raw{};
};

class users_api final {
public:
    users_api() = default;
    explicit users_api(void* state);
    ~users_api();
    users_api(const users_api& other);
    users_api& operator=(const users_api& other);
    users_api(users_api&& other) noexcept;
    users_api& operator=(users_api&& other) noexcept;
    [[nodiscard]] task<result<farcaster_user>> get_by_fid(std::uint64_t fid, call_options options = {}) const;
    [[nodiscard]] task<result<farcaster_user>> get_by_username(std::string username, call_options options = {}) const;
    [[nodiscard]] task<result<std::vector<farcaster_user>>> search(std::string text, std::uint32_t limit = 10, call_options options = {}) const;
private: void* state_{};
};

class casts_api final {
public:
    casts_api() = default;
    explicit casts_api(void* state);
    ~casts_api();
    casts_api(const casts_api& other);
    casts_api& operator=(const casts_api& other);
    casts_api(casts_api&& other) noexcept;
    casts_api& operator=(casts_api&& other) noexcept;
    [[nodiscard]] task<result<farcaster_cast>> get(std::string hash, call_options options = {}) const;
    [[nodiscard]] task<result<json::value>> conversation(std::string hash, std::uint32_t reply_depth = 2, call_options options = {}) const;
    [[nodiscard]] task<result<std::vector<farcaster_cast>>> search(std::string text, std::uint32_t limit = 10, call_options options = {}) const;
private: void* state_{};
};

class feeds_api final {
public:
    feeds_api() = default;
    explicit feeds_api(void* state);
    ~feeds_api();
    feeds_api(const feeds_api& other);
    feeds_api& operator=(const feeds_api& other);
    feeds_api(feeds_api&& other) noexcept;
    feeds_api& operator=(feeds_api&& other) noexcept;
    [[nodiscard]] task<result<feed_page>> following(std::uint64_t fid, std::uint32_t limit = 20, std::string cursor = {}, call_options options = {}) const;
    [[nodiscard]] task<result<feed_page>> trending(std::uint32_t limit = 20, std::string cursor = {}, call_options options = {}) const;
    [[nodiscard]] task<result<feed_page>> user_casts(std::uint64_t fid, std::uint32_t limit = 20, std::string cursor = {}, call_options options = {}) const;
private: void* state_{};
};

class hypersnap_api final {
public:
    hypersnap_api() = default;
    explicit hypersnap_api(void* state);
    ~hypersnap_api();
    hypersnap_api(const hypersnap_api& other);
    hypersnap_api& operator=(const hypersnap_api& other);
    hypersnap_api(hypersnap_api&& other) noexcept;
    hypersnap_api& operator=(hypersnap_api&& other) noexcept;
    [[nodiscard]] users_api users() const;
    [[nodiscard]] casts_api casts() const;
    [[nodiscard]] feeds_api feeds() const;
    [[nodiscard]] task<result<service_response>> get(std::string path,std::vector<std::pair<std::string,std::string>> query={},call_options options={}) const;
    [[nodiscard]] task<result<service_response>> post(std::string path,bytes json_payload,http_headers headers={},call_options options={}) const;
private: void* state_{};
};

/** High-level QStorage/S3-compatible application API. */
class storage_api final {
public:
    storage_api() = default;
    explicit storage_api(void* state);
    ~storage_api();
    storage_api(const storage_api& other);
    storage_api& operator=(const storage_api& other);
    storage_api(storage_api&& other) noexcept;
    storage_api& operator=(storage_api&& other) noexcept;
    [[nodiscard]] task<result<service_response>> put(std::string bucket,std::string key,bytes data,std::string content_type="application/octet-stream",call_options options={}) const;
    [[nodiscard]] task<result<service_response>> get(std::string bucket,std::string key,call_options options={}) const;
    [[nodiscard]] task<result<service_response>> remove(std::string bucket,std::string key,call_options options={}) const;
    [[nodiscard]] task<result<service_response>> create_multipart_upload(std::string bucket,std::string key,std::string content_type="application/octet-stream",call_options options={}) const;
    [[nodiscard]] task<result<service_response>> upload_part(std::string bucket,std::string key,std::string upload_id,std::uint32_t part_number,bytes data,call_options options={}) const;
    [[nodiscard]] task<result<service_response>> complete_multipart_upload(std::string bucket,std::string key,std::string upload_id,bytes completion_xml,call_options options={}) const;
    [[nodiscard]] task<result<service_response>> abort_multipart_upload(std::string bucket,std::string key,std::string upload_id,call_options options={}) const;
    /** Executes an arbitrary signed S3-compatible request target. */
    [[nodiscard]] task<result<service_response>> execute(http_method verb,std::string target,http_headers headers={},bytes body={},call_options options={}) const;
private: void* state_{};
};

/** High-level QKMS application API. */
class kms_api final {
public:
    kms_api() = default;
    explicit kms_api(void* state);
    ~kms_api();
    kms_api(const kms_api& other);
    kms_api& operator=(const kms_api& other);
    kms_api(kms_api&& other) noexcept;
    kms_api& operator=(kms_api&& other) noexcept;
    /** Invokes any compatible `TrentService.<operation>` QKMS operation. */
    [[nodiscard]] task<result<service_response>> invoke(std::string operation,std::string json_payload="{}",call_options options={}) const;
    [[nodiscard]] task<result<service_response>> create_key(std::string json_payload="{}",call_options options={}) const;
    [[nodiscard]] task<result<service_response>> describe_key(std::string json_payload,call_options options={}) const;
    [[nodiscard]] task<result<service_response>> encrypt(std::string json_payload,call_options options={}) const;
    [[nodiscard]] task<result<service_response>> decrypt(std::string json_payload,call_options options={}) const;
    [[nodiscard]] task<result<service_response>> sign(std::string json_payload,call_options options={}) const;
    [[nodiscard]] task<result<service_response>> verify(std::string json_payload,call_options options={}) const;
private: void* state_{};
};

/** Raw-protobuf unary gRPC API for Quilibrium protocol services. */
class native_api final {
public:
    native_api() = default;
    explicit native_api(void* state);
    ~native_api();
    native_api(const native_api& other);
    native_api& operator=(const native_api& other);
    native_api(native_api&& other) noexcept;
    native_api& operator=(native_api&& other) noexcept;
    [[nodiscard]] task<result<bytes>> call(native_service service,std::string method,bytes protobuf_payload={},call_options options={}) const;
private: void* state_{};
};

class sdk final {
public:
    sdk() = default;
    ~sdk();
    sdk(const sdk& other);
    sdk& operator=(const sdk& other);
    sdk(sdk&& other) noexcept;
    sdk& operator=(sdk&& other) noexcept;
    [[nodiscard]] hypersnap_api hypersnap() const;
    [[nodiscard]] storage_api storage() const;
    [[nodiscard]] kms_api kms() const;
    [[nodiscard]] native_api native() const;
    [[nodiscard]] bool has_native_protocol() const noexcept;
    [[nodiscard]] http_transport_ptr transport() const noexcept;
private:
    friend result<sdk> connect(sdk_config config);
    void* state_{};
};

/** Creates a ready-to-use SDK without performing network I/O. */
[[nodiscard]] result<sdk> connect(sdk_config config = {});

} // namespace quilibrium
