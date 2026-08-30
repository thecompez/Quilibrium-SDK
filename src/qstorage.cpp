module;
#include <algorithm>
#include <chrono>
#include <coroutine>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module quilibrium.qstorage;
import quilibrium.sigv4;

namespace quilibrium::qstorage {
namespace {
std::string path_for(std::string_view bucket, std::string_view key = {}) {
    std::string out = "/";
    out += percent_encode(bucket);
    if (!key.empty()) {
        out += "/";
        out += percent_encode(key, true);
    }
    return out;
}
raw_response convert(http_response value) {
    return {.status_code=value.status_code,.headers=std::move(value.header_fields),.body=std::move(value.body)};
}
}

struct client::impl final {
    std::vector<endpoint> endpoints;
    auth::sigv4_signer signer;
    http_transport_ptr transport;
    impl(config c,http_transport_ptr t)
        : endpoints(std::move(c.endpoints)),
          signer(auth::sigv4_credentials{.access_key_id=std::move(c.auth.access_key_id),.secret_access_key=std::move(c.auth.secret_access_key),.session_token=std::move(c.auth.session_token)},std::move(c.region),"s3"),
          transport(std::move(t)) {}
};

client::client(config c,http_transport_ptr t):impl_(std::make_unique<impl>(std::move(c),std::move(t))){}
client::~client()=default;
client::client(client&&) noexcept=default;
client& client::operator=(client&&) noexcept=default;

task<result<raw_response>> client::execute(http_method verb,std::string target,http_headers headers,bytes body,call_options options) {
    if(!impl_->transport) co_return std::unexpected(error{.domain=error_domain::configuration,.code=300,.message="HTTP transport is not configured"});
    if(impl_->endpoints.empty()) co_return std::unexpected(error{.domain=error_domain::configuration,.code=301,.message="QStorage endpoint list is empty"});

    const auto attempts = std::max<std::uint32_t>(1U, options.max_attempts);
    error last_error{.domain=error_domain::transport,.code=302,.message="QStorage request failed",.retryable=true};
    for(std::uint32_t attempt=0; attempt<attempts; ++attempt) {
        const auto& selected = impl_->endpoints[attempt % impl_->endpoints.size()];
        http_request request{.verb=verb,.target_endpoint=selected,.target=target,.header_fields=headers,.body=body};
        if(auto signed_status=impl_->signer.sign(request);!signed_status) co_return std::unexpected(signed_status.error());
        auto response=impl_->transport->send_now(std::move(request),options);
        if(!response) {
            last_error=response.error();
            if(!options.allow_failover || !last_error.retryable) co_return std::unexpected(last_error);
            continue;
        }
        if(http_retryable(response->status_code) && options.allow_failover && attempt + 1U < attempts) {
            last_error=error{.domain=error_domain::service,.code=response->status_code,.message="QStorage returned a retryable HTTP status",.http_status=response->status_code,.retryable=true};
        } else {
            co_return convert(std::move(*response));
        }
    }
    co_return std::unexpected(last_error);
}

task<result<raw_response>> client::create_bucket(std::string b,call_options o){co_return sync_wait(execute(http_method::put,path_for(b),{},{},o));}
task<result<raw_response>> client::delete_bucket(std::string b,call_options o){co_return sync_wait(execute(http_method::del,path_for(b),{},{},o));}
task<result<raw_response>> client::list_buckets(call_options o){co_return sync_wait(execute(http_method::get,"/",{},{},o));}
task<result<raw_response>> client::head_bucket(std::string b,call_options o){co_return sync_wait(execute(http_method::head,path_for(b),{},{},o));}
task<result<raw_response>> client::put_object(std::string b,object v,call_options o){http_headers h{{"content-type",v.content_type}};co_return sync_wait(execute(http_method::put,path_for(b,v.key),std::move(h),std::move(v.data),o));}
task<result<raw_response>> client::get_object(std::string b,std::string k,call_options o){co_return sync_wait(execute(http_method::get,path_for(b,k),{},{},o));}
task<result<raw_response>> client::delete_object(std::string b,std::string k,call_options o){co_return sync_wait(execute(http_method::del,path_for(b,k),{},{},o));}
task<result<raw_response>> client::head_object(std::string b,std::string k,call_options o){co_return sync_wait(execute(http_method::head,path_for(b,k),{},{},o));}
task<result<raw_response>> client::copy_object(std::string b,std::string k,std::string source,call_options o){http_headers h{{"x-amz-copy-source",std::move(source)}};co_return sync_wait(execute(http_method::put,path_for(b,k),std::move(h),{},o));}
task<result<raw_response>> client::list_objects(std::string b,std::string q,call_options o){std::string target=path_for(b);if(!q.empty())target+="?"+q;co_return sync_wait(execute(http_method::get,std::move(target),{},{},o));}

task<result<raw_response>> client::create_multipart_upload(std::string b,std::string k,std::string content_type,call_options o){
    http_headers h{{"content-type",std::move(content_type)}};
    co_return sync_wait(execute(http_method::post,path_for(b,k)+"?uploads",std::move(h),{},o));
}
task<result<raw_response>> client::upload_part(std::string b,std::string k,std::string upload_id,std::uint32_t part_number,bytes data,call_options o){
    auto target=path_for(b,k)+"?partNumber="+std::to_string(part_number)+"&uploadId="+percent_encode(upload_id);
    co_return sync_wait(execute(http_method::put,std::move(target),{},std::move(data),o));
}
task<result<raw_response>> client::complete_multipart_upload(std::string b,std::string k,std::string upload_id,bytes completion_xml,call_options o){
    auto target=path_for(b,k)+"?uploadId="+percent_encode(upload_id);
    http_headers h{{"content-type","application/xml"}};
    co_return sync_wait(execute(http_method::post,std::move(target),std::move(h),std::move(completion_xml),o));
}
task<result<raw_response>> client::abort_multipart_upload(std::string b,std::string k,std::string upload_id,call_options o){
    auto target=path_for(b,k)+"?uploadId="+percent_encode(upload_id);
    co_return sync_wait(execute(http_method::del,std::move(target),{},{},o));
}
task<result<raw_response>> client::list_parts(std::string b,std::string k,std::string upload_id,call_options o){
    auto target=path_for(b,k)+"?uploadId="+percent_encode(upload_id);
    co_return sync_wait(execute(http_method::get,std::move(target),{},{},o));
}
task<result<raw_response>> client::list_multipart_uploads(std::string b,call_options o){
    co_return sync_wait(execute(http_method::get,path_for(b)+"?uploads",{},{},o));
}

} // namespace quilibrium::qstorage
