module;
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <coroutine>
#include <expected>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module quilibrium.sdk;
import quilibrium.sigv4;
import quilibrium.transport.curl;

namespace quilibrium {
namespace {

struct route_entry final {
    endpoint value{};
    double latency_ewma_ms{0.0};
    std::uint32_t consecutive_failures{0};
    std::chrono::steady_clock::time_point quarantine_until{};
};

class route_pool final {
public:
    route_pool() = default;
    explicit route_pool(std::vector<endpoint> endpoints) {
        entries_.reserve(endpoints.size());
        for (auto& value : endpoints) entries_.push_back(route_entry{.value=std::move(value)});
    }

    [[nodiscard]] result<endpoint> select() const {
        std::scoped_lock lock{mutex_};
        if (entries_.empty()) {
            return std::unexpected(error{.domain=error_domain::configuration,.code=700,.message="endpoint pool is empty"});
        }
        const auto now=std::chrono::steady_clock::now();
        const route_entry* best=nullptr;
        double best_score=std::numeric_limits<double>::infinity();
        for (const auto& item:entries_) {
            if (item.quarantine_until>now) continue;
            const double latency=item.latency_ewma_ms>0.0?item.latency_ewma_ms:500.0;
            const double score=latency*(1.0+static_cast<double>(item.consecutive_failures)*0.75);
            if (score<best_score) { best=&item; best_score=score; }
        }
        if (!best) {
            best=&*std::min_element(entries_.begin(),entries_.end(),[](const auto& lhs,const auto& rhs){return lhs.quarantine_until<rhs.quarantine_until;});
        }
        return best->value;
    }

    void success(const endpoint& value,std::chrono::milliseconds latency) {
        std::scoped_lock lock{mutex_};
        for (auto& item:entries_) {
            if (item.value==value) {
                const auto sample=static_cast<double>(latency.count());
                item.latency_ewma_ms=item.latency_ewma_ms==0.0?sample:(0.2*sample+0.8*item.latency_ewma_ms);
                item.consecutive_failures=0;
                item.quarantine_until={};
                return;
            }
        }
    }

    void failure(const endpoint& value) {
        std::scoped_lock lock{mutex_};
        for (auto& item:entries_) {
            if (item.value==value) {
                ++item.consecutive_failures;
                const auto shift=std::min<std::uint32_t>(item.consecutive_failures,6U);
                item.quarantine_until=std::chrono::steady_clock::now()+std::chrono::seconds{1U<<shift};
                return;
            }
        }
    }

    [[nodiscard]] bool empty() const {
        std::scoped_lock lock{mutex_};
        return entries_.empty();
    }

private:
    mutable std::mutex mutex_{};
    std::vector<route_entry> entries_{};
};

struct sdk_state final {
    std::atomic_size_t refs{1};
    http_transport_ptr http{};
    route_pool hypersnap{};
    route_pool storage{};
    route_pool kms{};
    route_pool protocol{};
    std::optional<sdk_credentials> storage_credentials{};
    std::optional<sdk_credentials> kms_credentials{};
    std::string storage_region{"q"};
    std::string kms_region{"q"};

    sdk_state(http_transport_ptr transport,sdk_config config)
        : http(std::move(transport)),
          hypersnap(std::move(config.hypersnap_endpoints)),
          storage(std::move(config.qstorage_endpoints)),
          kms(std::move(config.qkms_endpoints)),
          protocol(std::move(config.protocol_endpoints)),
          storage_credentials(std::move(config.qstorage_credentials)),
          kms_credentials(std::move(config.qkms_credentials)),
          storage_region(std::move(config.qstorage_region)) {}
};

sdk_state* state_cast(void* value) noexcept { return static_cast<sdk_state*>(value); }

void retain_state(void* value) noexcept {
    if (auto* state=state_cast(value)) state->refs.fetch_add(1,std::memory_order_relaxed);
}

void release_state(void* value) noexcept {
    if (auto* state=state_cast(value)) {
        if (state->refs.fetch_sub(1,std::memory_order_acq_rel)==1) delete state;
    }
}

#define QL_DEFINE_STATE_LIFETIME(Type) \
    Type::Type(void* state):state_(state){retain_state(state_);} \
    Type::~Type(){release_state(state_);} \
    Type::Type(const Type& other):state_(other.state_){retain_state(state_);} \
    Type& Type::operator=(const Type& other){if(this!=&other){retain_state(other.state_);release_state(state_);state_=other.state_;}return *this;} \
    Type::Type(Type&& other) noexcept:state_(std::exchange(other.state_,nullptr)){} \
    Type& Type::operator=(Type&& other) noexcept{if(this!=&other){release_state(state_);state_=std::exchange(other.state_,nullptr);}return *this;}

[[nodiscard]] endpoint make_endpoint(std::string host) { return {.scheme="https",.host=std::move(host),.port=443}; }

[[nodiscard]] std::string encode_query(const std::vector<std::pair<std::string,std::string>>& parameters) {
    std::string out;
    for (const auto& [key,value]:parameters) {
        if (!out.empty()) out+='&';
        out+=percent_encode(key);
        out+='=';
        out+=percent_encode(value);
    }
    return out;
}

[[nodiscard]] result<http_response> send_with_failover(
    sdk_state& state,
    route_pool& pool,
    http_method verb,
    std::string target,
    http_headers headers,
    bytes body,
    call_options options,
    const std::optional<sdk_credentials>* credentials=nullptr,
    std::string_view region={},
    std::string_view signing_service={}) {

    if (!state.http || !*state.http) {
        return std::unexpected(error{.domain=error_domain::configuration,.code=701,.message="HTTP transport is not configured"});
    }
    const auto attempts=std::max<std::uint32_t>(1U,options.max_attempts);
    error last_error{.domain=error_domain::transport,.code=702,.message="request failed",.retryable=true};

    for (std::uint32_t attempt=0;attempt<attempts;++attempt) {
        auto selected=pool.select();
        if (!selected) return std::unexpected(selected.error());
        http_request request{.verb=verb,.target_endpoint=*selected,.target=target,.header_fields=headers,.body=body};
        if (credentials) {
            if (!credentials->has_value()) {
                return std::unexpected(error{.domain=error_domain::authentication,.code=703,.message="service credentials are not configured"});
            }
            const auto& c=credentials->value();
            auth::sigv4_signer signer(
                auth::sigv4_credentials{.access_key_id=c.access_key_id,.secret_access_key=c.secret_access_key,.session_token=c.session_token},
                std::string(region),std::string(signing_service));
            if (auto signed_status=signer.sign(request);!signed_status) return std::unexpected(signed_status.error());
        }

        const auto start=std::chrono::steady_clock::now();
        auto response=state.http->send_now(std::move(request),options);
        const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start);
        if (!response) {
            last_error=response.error();
            pool.failure(*selected);
            if (!options.allow_failover || !last_error.retryable || !options.idempotent) return std::unexpected(last_error);
            continue;
        }
        if (http_retryable(response->status_code) && options.allow_failover && options.idempotent && attempt+1U<attempts) {
            last_error=error{.domain=error_domain::service,.code=response->status_code,.message="upstream returned a retryable HTTP status",.http_status=response->status_code,.retryable=true};
            pool.failure(*selected);
            continue;
        }
        pool.success(*selected,elapsed);
        return response;
    }
    return std::unexpected(last_error);
}

[[nodiscard]] result<http_response> hypersnap_request(
    sdk_state& state,http_method verb,std::string path,
    std::vector<std::pair<std::string,std::string>> query={},
    http_headers headers={},bytes body={},call_options options={}) {
    if (const auto encoded=encode_query(query);!encoded.empty()) path+=(path.contains('?')?"&":"?")+encoded;
    if (verb==http_method::post) headers.try_emplace("content-type","application/json");
    return send_with_failover(state,state.hypersnap,verb,std::move(path),std::move(headers),std::move(body),options);
}

[[nodiscard]] error response_error(std::string_view service_name,std::int32_t status,byte_view body) {
    std::string message=std::string(service_name)+" returned HTTP "+std::to_string(status);
    const auto text=as_string(body);
    if (!text.empty()) message+=": "+text.substr(0,std::min<std::size_t>(text.size(),512U));
    return {.domain=error_domain::service,.code=status,.message=std::move(message),.http_status=status,.retryable=http_retryable(status)};
}

[[nodiscard]] result<json::value> parse_json_response(const http_response& response) {
    if (!http_success(response.status_code)) return std::unexpected(response_error("HyperSnap",response.status_code,response.body));
    return json::parse(as_string(response.body));
}

[[nodiscard]] farcaster_user parse_user_value(const json::value& value) {
    farcaster_user out{};
    out.fid=value.uint64_or("fid");
    out.username=value.string_or("username");
    out.display_name=value.string_or("display_name");
    out.pfp_url=value.string_or("pfp_url");
    out.follower_count=value.uint64_or("follower_count");
    out.following_count=value.uint64_or("following_count");
    if (const auto* profile=value.find("profile")) {
        if (const auto* bio=profile->find("bio")) {
            if (const auto* text=bio->as_string()) out.bio=*text;
            else out.bio=bio->string_or("text");
        }
    }
    if (const auto* verified=value.find("verified_addresses")) {
        for (std::string_view key:{"eth_addresses","sol_addresses","primary"}) {
            const auto* addresses=verified->find(key);
            if (!addresses) continue;
            if (const auto* list=addresses->as_array()) {
                for (const auto& item:*list) if (const auto* text=item.as_string()) out.verified_addresses.push_back(*text);
            } else if (const auto* text=addresses->as_string()) out.verified_addresses.push_back(*text);
        }
    }
    out.raw=value;
    return out;
}

[[nodiscard]] farcaster_cast parse_cast_value(const json::value& value) {
    farcaster_cast out{};
    out.hash=value.string_or("hash");
    out.text=value.string_or("text");
    out.timestamp=value.string_or("timestamp");
    out.parent_hash=value.string_or("parent_hash");
    out.parent_url=value.string_or("parent_url");
    out.root_parent_url=value.string_or("root_parent_url");
    if (const auto* author=value.find("author")) out.author=parse_user_value(*author);
    if (const auto* reactions=value.find("reactions")) {
        out.likes=reactions->uint64_or("likes_count",reactions->uint64_or("likes"));
        out.recasts=reactions->uint64_or("recasts_count",reactions->uint64_or("recasts"));
    }
    if (const auto* replies=value.find("replies")) out.replies=replies->uint64_or("count");
    out.raw=value;
    return out;
}

[[nodiscard]] result<farcaster_user> parse_user_response(const http_response& response) {
    auto root=parse_json_response(response);
    if (!root) return std::unexpected(root.error());
    const auto* user=root->find("user");
    if (!user) return std::unexpected(error{.domain=error_domain::protocol,.code=800,.message="HyperSnap user response is missing 'user'"});
    return parse_user_value(*user);
}

[[nodiscard]] result<std::vector<farcaster_user>> parse_users_response(const http_response& response) {
    auto root=parse_json_response(response);
    if (!root) return std::unexpected(root.error());
    std::vector<farcaster_user> out;
    if (const auto* users=root->find("users")) if (const auto* list=users->as_array()) for (const auto& item:*list) out.push_back(parse_user_value(item));
    return out;
}

[[nodiscard]] result<farcaster_cast> parse_cast_response(const http_response& response) {
    auto root=parse_json_response(response);
    if (!root) return std::unexpected(root.error());
    const auto* cast=root->find("cast");
    if (!cast) return std::unexpected(error{.domain=error_domain::protocol,.code=801,.message="HyperSnap cast response is missing 'cast'"});
    return parse_cast_value(*cast);
}

[[nodiscard]] result<std::vector<farcaster_cast>> parse_casts_response(const http_response& response) {
    auto root=parse_json_response(response);
    if (!root) return std::unexpected(root.error());
    std::vector<farcaster_cast> out;
    if (const auto* casts=root->find("casts")) if (const auto* list=casts->as_array()) for (const auto& item:*list) out.push_back(parse_cast_value(item));
    return out;
}

[[nodiscard]] result<feed_page> parse_feed_response(const http_response& response) {
    auto root=parse_json_response(response);
    if (!root) return std::unexpected(root.error());
    feed_page page{};
    page.raw=*root;
    if (const auto* casts=root->find("casts")) if (const auto* list=casts->as_array()) for (const auto& item:*list) page.casts.push_back(parse_cast_value(item));
    if (const auto* next=root->find("next")) page.cursor=next->string_or("cursor");
    return page;
}

[[nodiscard]] service_response convert(http_response response) {
    return {.status_code=response.status_code,.headers=std::move(response.header_fields),.body=std::move(response.body)};
}

[[nodiscard]] result<presigned_url> to_presigned_url(result<auth::presigned_request> value) {
    if (!value) return std::unexpected(value.error());
    return presigned_url{
        .url=std::move(value->url),
        .required_headers=std::move(value->required_headers),
        .expires_at=value->expires_at
    };
}

[[nodiscard]] std::string storage_path(std::string_view bucket,std::string_view key={}) {
    std::string path="/";
    path+=percent_encode(bucket);
    if (!key.empty()) { path+='/'; path+=percent_encode(key,true); }
    return path;
}

[[nodiscard]] std::string grpc_service_name(native_service service) {
    switch (service) {
        case native_service::node:return "quilibrium.node.node.pb.NodeService";
        case native_service::connectivity:return "quilibrium.node.node.pb.ConnectivityService";
        case native_service::global:return "quilibrium.node.global.pb.GlobalService";
        case native_service::app_shard:return "quilibrium.node.global.pb.AppShardService";
        case native_service::hypergraph_comparison:return "quilibrium.node.application.pb.HypergraphComparisonService";
        case native_service::key_registry:return "quilibrium.node.global.pb.KeyRegistryService";
        case native_service::dispatch:return "quilibrium.node.global.pb.DispatchService";
        case native_service::mixnet:return "quilibrium.node.global.pb.MixnetService";
        case native_service::onion:return "quilibrium.node.global.pb.OnionService";
        case native_service::pubsub_proxy:return "quilibrium.node.proxy.pb.PubSubProxy";
        case native_service::data_ipc:return "quilibrium.node.node.pb.DataIPCService";
        case native_service::ferret_proxy:return "quilibrium.node.ferretproxy.pb.FerretProxy";
    }
    return "quilibrium.node.node.pb.NodeService";
}

[[nodiscard]] bytes grpc_frame(byte_view payload) {
    bytes framed(5U+payload.size());
    framed[0]=byte{0};
    const auto size=static_cast<std::uint32_t>(payload.size());
    framed[1]=static_cast<byte>((size>>24U)&0xFFU);
    framed[2]=static_cast<byte>((size>>16U)&0xFFU);
    framed[3]=static_cast<byte>((size>>8U)&0xFFU);
    framed[4]=static_cast<byte>(size&0xFFU);
    std::copy(payload.begin(),payload.end(),framed.begin()+5);
    return framed;
}

[[nodiscard]] result<bytes> grpc_unframe(byte_view body) {
    if (body.size()<5U) return std::unexpected(error{.domain=error_domain::protocol,.code=860,.message="gRPC response frame is truncated"});
    if (body[0]!=byte{0}) return std::unexpected(error{.domain=error_domain::protocol,.code=861,.message="compressed gRPC frames are not supported by the raw unary facade"});
    const auto u=[](byte value){return std::to_integer<std::uint32_t>(value);};
    const std::uint32_t length=(u(body[1])<<24U)|(u(body[2])<<16U)|(u(body[3])<<8U)|u(body[4]);
    if (body.size()<5U+length) return std::unexpected(error{.domain=error_domain::protocol,.code=862,.message="gRPC response payload length is invalid"});
    return bytes(body.begin()+5,body.begin()+5+length);
}

} // namespace

QL_DEFINE_STATE_LIFETIME(users_api)

task<result<farcaster_user>> users_api::get_by_fid(std::uint64_t fid,call_options options) const {
    auto* state=state_cast(state_);
    if (!state) co_return std::unexpected(error{.domain=error_domain::configuration,.code=810,.message="SDK state is unavailable"});
    auto response=hypersnap_request(*state,http_method::get,"/v2/farcaster/user",{{"fid",std::to_string(fid)}},{},{},options);
    if (!response) co_return std::unexpected(response.error());
    co_return parse_user_response(*response);
}

task<result<farcaster_user>> users_api::get_by_username(std::string username,call_options options) const {
    auto* state=state_cast(state_);
    if (!state) co_return std::unexpected(error{.domain=error_domain::configuration,.code=811,.message="SDK state is unavailable"});
    auto response=hypersnap_request(*state,http_method::get,"/v2/farcaster/user/by-username",{{"username",std::move(username)}},{},{},options);
    if (!response) co_return std::unexpected(response.error());
    co_return parse_user_response(*response);
}

task<result<std::vector<farcaster_user>>> users_api::search(std::string text,std::uint32_t limit,call_options options) const {
    auto* state=state_cast(state_);
    if (!state) co_return std::unexpected(error{.domain=error_domain::configuration,.code=812,.message="SDK state is unavailable"});
    auto response=hypersnap_request(*state,http_method::get,"/v2/farcaster/user/search",{{"q",std::move(text)},{"limit",std::to_string(limit)}},{},{},options);
    if (!response) co_return std::unexpected(response.error());
    co_return parse_users_response(*response);
}

QL_DEFINE_STATE_LIFETIME(casts_api)

task<result<farcaster_cast>> casts_api::get(std::string hash,call_options options) const {
    auto* state=state_cast(state_);
    if (!state) co_return std::unexpected(error{.domain=error_domain::configuration,.code=820,.message="SDK state is unavailable"});
    auto response=hypersnap_request(*state,http_method::get,"/v2/farcaster/cast",{{"identifier",std::move(hash)},{"type","hash"}},{},{},options);
    if (!response) co_return std::unexpected(response.error());
    co_return parse_cast_response(*response);
}

task<result<json::value>> casts_api::conversation(std::string hash,std::uint32_t depth,call_options options) const {
    auto* state=state_cast(state_);
    if (!state) co_return std::unexpected(error{.domain=error_domain::configuration,.code=821,.message="SDK state is unavailable"});
    depth=std::min<std::uint32_t>(depth,5U);
    auto response=hypersnap_request(*state,http_method::get,"/v2/farcaster/cast/conversation",{{"identifier",std::move(hash)},{"type","hash"},{"reply_depth",std::to_string(depth)}},{},{},options);
    if (!response) co_return std::unexpected(response.error());
    co_return parse_json_response(*response);
}

task<result<std::vector<farcaster_cast>>> casts_api::search(std::string text,std::uint32_t limit,call_options options) const {
    auto* state=state_cast(state_);
    if (!state) co_return std::unexpected(error{.domain=error_domain::configuration,.code=822,.message="SDK state is unavailable"});
    auto response=hypersnap_request(*state,http_method::get,"/v2/farcaster/cast/search",{{"q",std::move(text)},{"limit",std::to_string(limit)}},{},{},options);
    if (!response) co_return std::unexpected(response.error());
    co_return parse_casts_response(*response);
}

QL_DEFINE_STATE_LIFETIME(feeds_api)

task<result<feed_page>> feeds_api::following(std::uint64_t fid,std::uint32_t limit,std::string cursor,call_options options) const {
    auto* state=state_cast(state_);
    if (!state) co_return std::unexpected(error{.domain=error_domain::configuration,.code=830,.message="SDK state is unavailable"});
    std::vector<std::pair<std::string,std::string>> query{{"fid",std::to_string(fid)},{"limit",std::to_string(limit)}};
    if (!cursor.empty()) query.emplace_back("cursor",std::move(cursor));
    auto response=hypersnap_request(*state,http_method::get,"/v2/farcaster/feed/following",std::move(query),{},{},options);
    if (!response) co_return std::unexpected(response.error());
    co_return parse_feed_response(*response);
}

task<result<feed_page>> feeds_api::trending(std::uint32_t limit,std::string cursor,call_options options) const {
    auto* state=state_cast(state_);
    if (!state) co_return std::unexpected(error{.domain=error_domain::configuration,.code=831,.message="SDK state is unavailable"});
    std::vector<std::pair<std::string,std::string>> query{{"limit",std::to_string(limit)}};
    if (!cursor.empty()) query.emplace_back("cursor",std::move(cursor));
    auto response=hypersnap_request(*state,http_method::get,"/v2/farcaster/feed/trending",std::move(query),{},{},options);
    if (!response) co_return std::unexpected(response.error());
    co_return parse_feed_response(*response);
}

task<result<feed_page>> feeds_api::user_casts(std::uint64_t fid,std::uint32_t limit,std::string cursor,call_options options) const {
    auto* state=state_cast(state_);
    if (!state) co_return std::unexpected(error{.domain=error_domain::configuration,.code=832,.message="SDK state is unavailable"});
    std::vector<std::pair<std::string,std::string>> query{{"fid",std::to_string(fid)},{"limit",std::to_string(limit)}};
    if (!cursor.empty()) query.emplace_back("cursor",std::move(cursor));
    auto response=hypersnap_request(*state,http_method::get,"/v2/farcaster/feed/user/casts",std::move(query),{},{},options);
    if (!response) co_return std::unexpected(response.error());
    co_return parse_feed_response(*response);
}

QL_DEFINE_STATE_LIFETIME(hypersnap_api)
users_api hypersnap_api::users() const { return users_api{state_}; }
casts_api hypersnap_api::casts() const { return casts_api{state_}; }
feeds_api hypersnap_api::feeds() const { return feeds_api{state_}; }

task<result<service_response>> hypersnap_api::get(std::string path,std::vector<std::pair<std::string,std::string>> query,call_options options) const {
    auto* state=state_cast(state_);
    if (!state) co_return std::unexpected(error{.domain=error_domain::configuration,.code=833,.message="SDK state is unavailable"});
    auto response=hypersnap_request(*state,http_method::get,std::move(path),std::move(query),{},{},options);
    if (!response) co_return std::unexpected(response.error());
    co_return convert(std::move(*response));
}

task<result<service_response>> hypersnap_api::post(std::string path,bytes body,http_headers headers,call_options options) const {
    auto* state=state_cast(state_);
    if (!state) co_return std::unexpected(error{.domain=error_domain::configuration,.code=834,.message="SDK state is unavailable"});
    auto response=hypersnap_request(*state,http_method::post,std::move(path),{},std::move(headers),std::move(body),options);
    if (!response) co_return std::unexpected(response.error());
    co_return convert(std::move(*response));
}

QL_DEFINE_STATE_LIFETIME(storage_api)

task<result<service_response>> storage_api::put(std::string bucket,std::string key,bytes data,std::string content_type,call_options options) const {
    auto* state=state_cast(state_);
    if (!state) co_return std::unexpected(error{.domain=error_domain::configuration,.code=840,.message="SDK state is unavailable"});
    auto response=send_with_failover(*state,state->storage,http_method::put,storage_path(bucket,key),{{"content-type",std::move(content_type)}},std::move(data),options,&state->storage_credentials,state->storage_region,"s3");
    if (!response) co_return std::unexpected(response.error());
    co_return convert(std::move(*response));
}

task<result<service_response>> storage_api::get(std::string bucket,std::string key,call_options options) const {
    auto* state=state_cast(state_);
    if (!state) co_return std::unexpected(error{.domain=error_domain::configuration,.code=841,.message="SDK state is unavailable"});
    auto response=send_with_failover(*state,state->storage,http_method::get,storage_path(bucket,key),{},{},options,&state->storage_credentials,state->storage_region,"s3");
    if (!response) co_return std::unexpected(response.error());
    co_return convert(std::move(*response));
}

task<result<service_response>> storage_api::remove(std::string bucket,std::string key,call_options options) const {
    auto* state=state_cast(state_);
    if (!state) co_return std::unexpected(error{.domain=error_domain::configuration,.code=842,.message="SDK state is unavailable"});
    auto response=send_with_failover(*state,state->storage,http_method::del,storage_path(bucket,key),{},{},options,&state->storage_credentials,state->storage_region,"s3");
    if (!response) co_return std::unexpected(response.error());
    co_return convert(std::move(*response));
}

result<presigned_url> storage_api::presign_put(
    std::string bucket,
    std::string key,
    std::string content_type,
    std::chrono::seconds expires) const {
    auto* state=state_cast(state_);
    if (!state) {
        return std::unexpected(error{.domain=error_domain::configuration,.code=844,.message="SDK state is unavailable"});
    }
    if (!state->storage_credentials) {
        return std::unexpected(error{.domain=error_domain::authentication,.code=703,.message="service credentials are not configured"});
    }
    auto selected=state->storage.select();
    if (!selected) return std::unexpected(selected.error());
    const auto& credentials=*state->storage_credentials;
    auth::sigv4_signer signer(
        auth::sigv4_credentials{
            .access_key_id=credentials.access_key_id,
            .secret_access_key=credentials.secret_access_key,
            .session_token=credentials.session_token
        },
        state->storage_region,
        "s3");
    http_headers headers;
    auth::presign_options options{.expires=expires};
    if (!content_type.empty()) {
        headers.emplace("content-type",std::move(content_type));
        options.signed_headers.emplace_back("content-type");
    }
    return to_presigned_url(signer.presign(http_request{
        .verb=http_method::put,
        .target_endpoint=*selected,
        .target=storage_path(bucket,key),
        .header_fields=std::move(headers),
        .body={}
    },std::move(options)));
}

result<presigned_url> storage_api::presign_get(
    std::string bucket,
    std::string key,
    std::chrono::seconds expires) const {
    auto* state=state_cast(state_);
    if (!state) {
        return std::unexpected(error{.domain=error_domain::configuration,.code=845,.message="SDK state is unavailable"});
    }
    if (!state->storage_credentials) {
        return std::unexpected(error{.domain=error_domain::authentication,.code=703,.message="service credentials are not configured"});
    }
    auto selected=state->storage.select();
    if (!selected) return std::unexpected(selected.error());
    const auto& credentials=*state->storage_credentials;
    auth::sigv4_signer signer(
        auth::sigv4_credentials{
            .access_key_id=credentials.access_key_id,
            .secret_access_key=credentials.secret_access_key,
            .session_token=credentials.session_token
        },
        state->storage_region,
        "s3");
    return to_presigned_url(signer.presign(http_request{
        .verb=http_method::get,
        .target_endpoint=*selected,
        .target=storage_path(bucket,key),
        .header_fields={},
        .body={}
    },auth::presign_options{.expires=expires}));
}

result<presigned_url> storage_api::presign_head(
    std::string bucket,
    std::string key,
    std::chrono::seconds expires) const {
    auto* state=state_cast(state_);
    if (!state) {
        return std::unexpected(error{.domain=error_domain::configuration,.code=846,.message="SDK state is unavailable"});
    }
    if (!state->storage_credentials) {
        return std::unexpected(error{.domain=error_domain::authentication,.code=703,.message="service credentials are not configured"});
    }
    auto selected=state->storage.select();
    if (!selected) return std::unexpected(selected.error());
    const auto& credentials=*state->storage_credentials;
    auth::sigv4_signer signer(
        auth::sigv4_credentials{
            .access_key_id=credentials.access_key_id,
            .secret_access_key=credentials.secret_access_key,
            .session_token=credentials.session_token
        },
        state->storage_region,
        "s3");
    return to_presigned_url(signer.presign(http_request{
        .verb=http_method::head,
        .target_endpoint=*selected,
        .target=storage_path(bucket,key),
        .header_fields={},
        .body={}
    },auth::presign_options{.expires=expires}));
}

task<result<service_response>> storage_api::create_multipart_upload(std::string bucket,std::string key,std::string content_type,call_options options) const {
    co_return sync_wait(execute(http_method::post,storage_path(bucket,key)+"?uploads",{{"content-type",std::move(content_type)}},{},options));
}

task<result<service_response>> storage_api::upload_part(std::string bucket,std::string key,std::string upload_id,std::uint32_t part_number,bytes data,call_options options) const {
    auto target=storage_path(bucket,key)+"?partNumber="+std::to_string(part_number)+"&uploadId="+percent_encode(upload_id);
    co_return sync_wait(execute(http_method::put,std::move(target),{},std::move(data),options));
}

task<result<service_response>> storage_api::complete_multipart_upload(std::string bucket,std::string key,std::string upload_id,bytes completion_xml,call_options options) const {
    auto target=storage_path(bucket,key)+"?uploadId="+percent_encode(upload_id);
    co_return sync_wait(execute(http_method::post,std::move(target),{{"content-type","application/xml"}},std::move(completion_xml),options));
}

task<result<service_response>> storage_api::abort_multipart_upload(std::string bucket,std::string key,std::string upload_id,call_options options) const {
    auto target=storage_path(bucket,key)+"?uploadId="+percent_encode(upload_id);
    co_return sync_wait(execute(http_method::del,std::move(target),{},{},options));
}

task<result<service_response>> storage_api::execute(http_method verb,std::string target,http_headers headers,bytes body,call_options options) const {
    auto* state=state_cast(state_);
    if (!state) co_return std::unexpected(error{.domain=error_domain::configuration,.code=843,.message="SDK state is unavailable"});
    auto response=send_with_failover(*state,state->storage,verb,std::move(target),std::move(headers),std::move(body),options,&state->storage_credentials,state->storage_region,"s3");
    if (!response) co_return std::unexpected(response.error());
    co_return convert(std::move(*response));
}

QL_DEFINE_STATE_LIFETIME(kms_api)

task<result<service_response>> kms_api::invoke(std::string operation,std::string json_payload,call_options options) const {
    auto* state=state_cast(state_);
    if (!state) co_return std::unexpected(error{.domain=error_domain::configuration,.code=850,.message="SDK state is unavailable"});
    auto view=as_bytes(json_payload);
    bytes payload(view.begin(),view.end());
    auto response=send_with_failover(*state,state->kms,http_method::post,"/",{{"content-type","application/x-amz-json-1.1"},{"x-amz-target","TrentService."+operation}},std::move(payload),options,&state->kms_credentials,state->kms_region,"kms");
    if (!response) co_return std::unexpected(response.error());
    co_return convert(std::move(*response));
}

#define QL_SDK_KMS_FORWARD(name,operation) task<result<service_response>> kms_api::name(std::string payload,call_options options) const { co_return sync_wait(invoke(operation,std::move(payload),options)); }
QL_SDK_KMS_FORWARD(create_key,"CreateKey")
QL_SDK_KMS_FORWARD(describe_key,"DescribeKey")
QL_SDK_KMS_FORWARD(encrypt,"Encrypt")
QL_SDK_KMS_FORWARD(decrypt,"Decrypt")
QL_SDK_KMS_FORWARD(sign,"Sign")
QL_SDK_KMS_FORWARD(verify,"Verify")
#undef QL_SDK_KMS_FORWARD

QL_DEFINE_STATE_LIFETIME(native_api)

task<result<bytes>> native_api::call(native_service target,std::string method,bytes payload,call_options options) const {
    auto* state=state_cast(state_);
    if (!state) co_return std::unexpected(error{.domain=error_domain::configuration,.code=863,.message="SDK state is unavailable"});
    if (state->protocol.empty()) co_return std::unexpected(error{.domain=error_domain::configuration,.code=864,.message="native protocol endpoint list is empty"});
    const auto path="/"+grpc_service_name(target)+"/"+method;
    auto response=send_with_failover(*state,state->protocol,http_method::post,path,{{"content-type","application/grpc"},{"te","trailers"},{"grpc-accept-encoding","identity"}},grpc_frame(payload),options);
    if (!response) co_return std::unexpected(response.error());
    if (response->status_code!=200) co_return std::unexpected(response_error("Quilibrium gRPC",response->status_code,response->body));
    if (const auto it=response->header_fields.find("grpc-status");it!=response->header_fields.end()&&it->second!="0") {
        const auto message_it=response->header_fields.find("grpc-message");
        co_return std::unexpected(error{.domain=error_domain::protocol,.code=865,.message=message_it==response->header_fields.end()?"gRPC call failed":message_it->second,.upstream_code=it->second});
    }
    co_return grpc_unframe(response->body);
}

hypersnap_api sdk::hypersnap() const { return hypersnap_api{state_}; }
storage_api sdk::storage() const { return storage_api{state_}; }
kms_api sdk::kms() const { return kms_api{state_}; }
native_api sdk::native() const { return native_api{state_}; }

sdk::~sdk(){release_state(state_);}
sdk::sdk(const sdk& other):state_(other.state_){retain_state(state_);}
sdk& sdk::operator=(const sdk& other){if(this!=&other){retain_state(other.state_);release_state(state_);state_=other.state_;}return *this;}
sdk::sdk(sdk&& other) noexcept:state_(std::exchange(other.state_,nullptr)){}
sdk& sdk::operator=(sdk&& other) noexcept{if(this!=&other){release_state(state_);state_=std::exchange(other.state_,nullptr);}return *this;}

bool sdk::has_native_protocol() const noexcept { auto* state=state_cast(state_); return state&&!state->protocol.empty(); }
http_transport_ptr sdk::transport() const noexcept { auto* state=state_cast(state_); return state?state->http:http_transport_ptr{}; }

result<sdk> connect(sdk_config config) {
    if (config.hypersnap_endpoints.empty()) config.hypersnap_endpoints.push_back(make_endpoint("haatz.quilibrium.com"));
    if (config.qstorage_endpoints.empty()) config.qstorage_endpoints.push_back(make_endpoint("qstorage.quilibrium.com"));
    if (config.qkms_endpoints.empty()) config.qkms_endpoints.push_back(make_endpoint("qkms.quilibrium.com"));
    if (!config.http) config.http=transport::make_curl_http_transport(transport::curl_options{.user_agent=std::move(config.user_agent),.verify_tls=config.verify_tls,.follow_redirects=true});
    if (!config.http) return std::unexpected(error{.domain=error_domain::configuration,.code=870,.message="failed to create default HTTP transport"});
    auto http=config.http;
    auto* state=new sdk_state(std::move(http),std::move(config));
    sdk out{};
    out.state_=state;
    return out;
}

#undef QL_DEFINE_STATE_LIFETIME

} // namespace quilibrium
