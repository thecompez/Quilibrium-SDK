module;
#include <array>
#include <algorithm>
#include <chrono>
#include <coroutine>
#include <cstdint>
#include <expected>
#include <memory>
#include <openssl/crypto.h>
#include <openssl/hmac.h>
#include <string>
#include <utility>

module quilibrium.hypersnap;
import quilibrium.net;

namespace quilibrium::hypersnap {
namespace {

std::string encode_query(const query& parameters) {
    std::string out;
    for (const auto& [key,value] : parameters) {
        if (!out.empty()) out += '&';
        out += percent_encode(key);
        out += '=';
        out += percent_encode(value);
    }
    return out;
}

response convert(http_response value) {
    return {.status_code=value.status_code,.headers=std::move(value.header_fields),.json=std::move(value.body)};
}

} // namespace

struct client::impl final {
    net::endpoint_pool endpoints;
    http_transport_ptr transport;
    impl(config configuration,http_transport_ptr value_transport)
        : endpoints(std::move(configuration.endpoints)),transport(std::move(value_transport)) {}
};

client::client(config configuration,http_transport_ptr transport)
    : impl_(std::make_unique<impl>(std::move(configuration),std::move(transport))) {}
client::~client()=default;
client::client(client&&) noexcept=default;
client& client::operator=(client&&) noexcept=default;

task<result<response>> client::get(std::string path,query parameters,call_options options) {
    if (!impl_->transport) co_return std::unexpected(error{.domain=error_domain::configuration,.code=500,.message="HTTP transport is not configured"});
    if (const auto encoded=encode_query(parameters); !encoded.empty()) path += (path.contains('?') ? "&" : "?") + encoded;
    const auto attempts = std::max<std::uint32_t>(1U, options.max_attempts);
    error last_error{.domain=error_domain::transport,.code=502,.message="request failed",.retryable=true};
    for (std::uint32_t attempt=0; attempt<attempts; ++attempt) {
        auto selected=impl_->endpoints.select();
        if(!selected) co_return std::unexpected(selected.error());
        const auto start=std::chrono::steady_clock::now();
        auto raw=impl_->transport->send_now(http_request{.verb=http_method::get,.target_endpoint=*selected,.target=path},options);
        const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start);
        if(!raw) {
            last_error=raw.error();
            impl_->endpoints.report_failure(*selected);
            if (!options.allow_failover || !last_error.retryable) co_return std::unexpected(last_error);
            continue;
        }
        if (http_retryable(raw->status_code) && attempt + 1U < attempts && options.allow_failover) {
            impl_->endpoints.report_failure(*selected);
            continue;
        }
        impl_->endpoints.report_success(*selected,elapsed);
        co_return convert(std::move(*raw));
    }
    co_return std::unexpected(last_error);
}

task<result<response>> client::post(std::string path,bytes payload,http_headers headers,call_options options) {
    if(!impl_->transport) co_return std::unexpected(error{.domain=error_domain::configuration,.code=501,.message="HTTP transport is not configured"});
    headers.try_emplace("content-type","application/json");
    const auto attempts = std::max<std::uint32_t>(1U, options.max_attempts);
    error last_error{.domain=error_domain::transport,.code=503,.message="request failed",.retryable=true};
    for (std::uint32_t attempt=0; attempt<attempts; ++attempt) {
        auto selected=impl_->endpoints.select();
        if(!selected) co_return std::unexpected(selected.error());
        const auto start=std::chrono::steady_clock::now();
        auto raw=impl_->transport->send_now(http_request{.verb=http_method::post,.target_endpoint=*selected,.target=path,.header_fields=headers,.body=payload},options);
        const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start);
        if(!raw) {
            last_error=raw.error(); impl_->endpoints.report_failure(*selected);
            if (!options.allow_failover || !last_error.retryable || !options.idempotent) co_return std::unexpected(last_error);
            continue;
        }
        if (http_retryable(raw->status_code) && attempt + 1U < attempts && options.allow_failover && options.idempotent) {
            impl_->endpoints.report_failure(*selected); continue;
        }
        impl_->endpoints.report_success(*selected,elapsed);
        co_return convert(std::move(*raw));
    }
    co_return std::unexpected(last_error);
}

task<result<response>> client::user_by_fid(std::uint64_t fid,call_options options) {
    co_return sync_wait(get("/v2/farcaster/user",{{"fid",std::to_string(fid)}},options));
}
task<result<response>> client::user_by_username(std::string username,call_options options) {
    co_return sync_wait(get("/v2/farcaster/user/by-username",{{"username",std::move(username)}},options));
}
task<result<response>> client::search_users(std::string text,std::uint32_t limit,call_options options) {
    co_return sync_wait(get("/v2/farcaster/user/search",{{"q",std::move(text)},{"limit",std::to_string(limit)}},options));
}
task<result<response>> client::cast(std::string identifier,call_options options) {
    co_return sync_wait(get("/v2/farcaster/cast",{{"identifier",std::move(identifier)},{"type","hash"}},options));
}
task<result<response>> client::cast_conversation(std::string identifier,std::uint32_t reply_depth,call_options options) {
    reply_depth=std::min<std::uint32_t>(reply_depth,5U);
    co_return sync_wait(get("/v2/farcaster/cast/conversation",{{"identifier",std::move(identifier)},{"type","hash"},{"reply_depth",std::to_string(reply_depth)}},options));
}
task<result<response>> client::search_casts(std::string text,std::uint32_t limit,call_options options) {
    co_return sync_wait(get("/v2/farcaster/cast/search",{{"q",std::move(text)},{"limit",std::to_string(limit)}},options));
}
task<result<response>> client::feed(query parameters,call_options options) {
    co_return sync_wait(get("/v2/farcaster/feed",std::move(parameters),options));
}
task<result<response>> client::following_feed(std::uint64_t fid,std::uint32_t limit,std::string cursor,call_options options) {
    query parameters{{"fid",std::to_string(fid)},{"limit",std::to_string(limit)}};
    if(!cursor.empty()) parameters.emplace_back("cursor",std::move(cursor));
    co_return sync_wait(get("/v2/farcaster/feed/following",std::move(parameters),options));
}
task<result<response>> client::trending_feed(std::uint32_t limit,std::string cursor,call_options options) {
    query parameters{{"limit",std::to_string(limit)}};
    if(!cursor.empty()) parameters.emplace_back("cursor",std::move(cursor));
    co_return sync_wait(get("/v2/farcaster/feed/trending",std::move(parameters),options));
}
task<result<response>> client::user_casts(std::uint64_t fid,std::uint32_t limit,std::string cursor,call_options options) {
    query parameters{{"fid",std::to_string(fid)},{"limit",std::to_string(limit)}};
    if(!cursor.empty()) parameters.emplace_back("cursor",std::move(cursor));
    co_return sync_wait(get("/v2/farcaster/feed/user/casts",std::move(parameters),options));
}

result<bool> verify_webhook_signature(std::string_view secret,byte_view payload,std::string_view signature) {
    std::array<unsigned char,EVP_MAX_MD_SIZE> digest{};
    unsigned int length=0;
    HMAC(EVP_sha512(),secret.data(),static_cast<int>(secret.size()),reinterpret_cast<const unsigned char*>(payload.data()),payload.size(),digest.data(),&length);
    const std::string expected=quilibrium::hex({reinterpret_cast<const byte*>(digest.data()),length});
    if(expected.size()!=signature.size()) return false;
    return CRYPTO_memcmp(expected.data(),signature.data(),expected.size())==0;
}

} // namespace quilibrium::hypersnap
