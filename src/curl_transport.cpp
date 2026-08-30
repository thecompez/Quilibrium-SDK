module;
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <curl/curl.h>
#include <expected>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module quilibrium.transport.curl;

namespace quilibrium::transport {
namespace {

void ensure_curl_global() {
    static std::once_flag flag;
    std::call_once(flag, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

std::string trim(std::string value) {
    auto is_space=[](unsigned char c){return std::isspace(c)!=0;};
    while(!value.empty()&&is_space(static_cast<unsigned char>(value.front())))value.erase(value.begin());
    while(!value.empty()&&is_space(static_cast<unsigned char>(value.back())))value.pop_back();
    return value;
}

struct transfer_context final { bytes body{}; http_headers headers{}; };
struct curl_http_context final { curl_options options{}; };

size_t write_body(char* ptr,size_t size,size_t nmemb,void* userdata){
    const auto total=size*nmemb;auto* ctx=static_cast<transfer_context*>(userdata);const auto* begin=reinterpret_cast<const byte*>(ptr);ctx->body.insert(ctx->body.end(),begin,begin+total);return total;
}
size_t write_header(char* ptr,size_t size,size_t nmemb,void* userdata){
    const auto total=size*nmemb;auto* ctx=static_cast<transfer_context*>(userdata);std::string_view line(ptr,total);const auto colon=line.find(':');
    if(colon!=std::string_view::npos){std::string key(line.substr(0,colon));std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});std::string value(line.substr(colon+1));while(!value.empty()&&(value.back()=='\r'||value.back()=='\n'))value.pop_back();ctx->headers[std::move(key)]=trim(std::move(value));}
    return total;
}

result<http_response> curl_http_send(void* opaque,http_request request,call_options options){
    auto* context=static_cast<curl_http_context*>(opaque);
    if(!context)return std::unexpected(error{.domain=error_domain::configuration,.code=599,.message="curl transport context is unavailable"});
    ensure_curl_global();
    CURL* handle=curl_easy_init();
    if(!handle)return std::unexpected(error{.domain=error_domain::transport,.code=600,.message="curl_easy_init failed",.retryable=true});

    transfer_context transfer{};curl_slist* header_list=nullptr;
    const std::string url=request.target_endpoint.origin()+request.target_endpoint.base_path+request.target;
    curl_easy_setopt(handle,CURLOPT_URL,url.c_str());
    curl_easy_setopt(handle,CURLOPT_USERAGENT,context->options.user_agent.c_str());
    curl_easy_setopt(handle,CURLOPT_TIMEOUT_MS,static_cast<long>(options.timeout.count()));
    curl_easy_setopt(handle,CURLOPT_CONNECTTIMEOUT_MS,static_cast<long>(std::min<std::int64_t>(options.timeout.count(),5'000)));
    curl_easy_setopt(handle,CURLOPT_FOLLOWLOCATION,context->options.follow_redirects?1L:0L);
    curl_easy_setopt(handle,CURLOPT_SSL_VERIFYPEER,context->options.verify_tls?1L:0L);
    curl_easy_setopt(handle,CURLOPT_SSL_VERIFYHOST,context->options.verify_tls?2L:0L);
    curl_easy_setopt(handle,CURLOPT_HTTP_VERSION,CURL_HTTP_VERSION_2TLS);
    curl_easy_setopt(handle,CURLOPT_ACCEPT_ENCODING,"");
    curl_easy_setopt(handle,CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(handle,CURLOPT_WRITEFUNCTION,&write_body);curl_easy_setopt(handle,CURLOPT_WRITEDATA,&transfer);
    curl_easy_setopt(handle,CURLOPT_HEADERFUNCTION,&write_header);curl_easy_setopt(handle,CURLOPT_HEADERDATA,&transfer);

    switch(request.verb){case http_method::get:curl_easy_setopt(handle,CURLOPT_HTTPGET,1L);break;case http_method::head:curl_easy_setopt(handle,CURLOPT_NOBODY,1L);break;case http_method::post:curl_easy_setopt(handle,CURLOPT_POST,1L);break;case http_method::put:curl_easy_setopt(handle,CURLOPT_CUSTOMREQUEST,"PUT");break;case http_method::del:curl_easy_setopt(handle,CURLOPT_CUSTOMREQUEST,"DELETE");break;case http_method::patch:curl_easy_setopt(handle,CURLOPT_CUSTOMREQUEST,"PATCH");break;}
    if(!request.body.empty()||request.verb==http_method::post||request.verb==http_method::put||request.verb==http_method::patch){curl_easy_setopt(handle,CURLOPT_POSTFIELDS,request.body.empty()?"":reinterpret_cast<const char*>(request.body.data()));curl_easy_setopt(handle,CURLOPT_POSTFIELDSIZE_LARGE,static_cast<curl_off_t>(request.body.size()));}
    for(const auto&[key,value]:request.header_fields){const std::string line=key+": "+value;header_list=curl_slist_append(header_list,line.c_str());}if(header_list)curl_easy_setopt(handle,CURLOPT_HTTPHEADER,header_list);

    const CURLcode rc=curl_easy_perform(handle);long status_code=0;curl_easy_getinfo(handle,CURLINFO_RESPONSE_CODE,&status_code);
    if (header_list) curl_slist_free_all(header_list);
    curl_easy_cleanup(handle);
    if(rc!=CURLE_OK){const bool retryable=rc==CURLE_OPERATION_TIMEDOUT||rc==CURLE_COULDNT_CONNECT||rc==CURLE_COULDNT_RESOLVE_HOST||rc==CURLE_RECV_ERROR||rc==CURLE_SEND_ERROR;return std::unexpected(error{.domain=error_domain::transport,.code=static_cast<std::int32_t>(rc),.message=curl_easy_strerror(rc),.retryable=retryable});}
    return http_response{.status_code=static_cast<std::int32_t>(status_code),.header_fields=std::move(transfer.headers),.body=std::move(transfer.body)};
}

std::string grpc_path(protocol::service service,std::string_view method){
    std::string service_name;switch(service){case protocol::service::node:service_name="quilibrium.node.node.pb.NodeService";break;case protocol::service::connectivity:service_name="quilibrium.node.node.pb.ConnectivityService";break;case protocol::service::global:service_name="quilibrium.node.global.pb.GlobalService";break;case protocol::service::app_shard:service_name="quilibrium.node.global.pb.AppShardService";break;case protocol::service::hypergraph_comparison:service_name="quilibrium.node.application.pb.HypergraphComparisonService";break;case protocol::service::key_registry:service_name="quilibrium.node.global.pb.KeyRegistryService";break;case protocol::service::dispatch:service_name="quilibrium.node.global.pb.DispatchService";break;case protocol::service::mixnet:service_name="quilibrium.node.global.pb.MixnetService";break;case protocol::service::onion:service_name="quilibrium.node.global.pb.OnionService";break;case protocol::service::pubsub_proxy:service_name="quilibrium.node.proxy.pb.PubSubProxy";break;case protocol::service::data_ipc:service_name="quilibrium.node.node.pb.DataIPCService";break;case protocol::service::ferret_proxy:service_name="quilibrium.node.ferretproxy.pb.FerretProxy";break;}return "/"+service_name+"/"+std::string(method);
}
bytes grpc_frame(byte_view payload){bytes framed(5+payload.size());framed[0]=byte{0};const auto size=static_cast<std::uint32_t>(payload.size());framed[1]=static_cast<byte>((size>>24U)&0xFFU);framed[2]=static_cast<byte>((size>>16U)&0xFFU);framed[3]=static_cast<byte>((size>>8U)&0xFFU);framed[4]=static_cast<byte>(size&0xFFU);std::copy(payload.begin(),payload.end(),framed.begin()+5);return framed;}
result<bytes> grpc_unframe(byte_view body){if(body.size()<5)return std::unexpected(error{.domain=error_domain::protocol,.code=610,.message="gRPC response frame is truncated"});if(body[0]!=byte{0})return std::unexpected(error{.domain=error_domain::protocol,.code=611,.message="compressed gRPC frames are not supported by the raw unary transport"});const auto u=[](byte v){return std::to_integer<std::uint32_t>(v);};const std::uint32_t length=(u(body[1])<<24U)|(u(body[2])<<16U)|(u(body[3])<<8U)|u(body[4]);if(body.size()<5U+length)return std::unexpected(error{.domain=error_domain::protocol,.code=612,.message="gRPC response payload length is invalid"});return bytes(body.begin()+5,body.begin()+5+length);}

class curl_grpc_transport final:public protocol::transport{
public:explicit curl_grpc_transport(http_transport_ptr http):http_(std::move(http)){}
    task<result<protocol::rpc_frame>> unary(endpoint target,protocol::rpc_call call,call_options options)override{
        if(call.mode!=protocol::streaming::unary)co_return std::unexpected(error{.domain=error_domain::protocol,.code=613,.message="this transport only implements unary gRPC calls"});
        if(!http_)co_return std::unexpected(error{.domain=error_domain::configuration,.code=614,.message="HTTP transport is not configured"});
        auto response=http_->send_now(http_request{.verb=http_method::post,.target_endpoint=std::move(target),.target=grpc_path(call.target_service,call.method),.header_fields={{"content-type","application/grpc"},{"te","trailers"},{"grpc-accept-encoding","identity"}},.body=grpc_frame(call.protobuf_payload)},options);
        if (!response) co_return std::unexpected(response.error());
        if (response->status_code != 200) co_return std::unexpected(error{.domain=error_domain::protocol,.code=615,.message="gRPC HTTP transport returned non-200 status",.http_status=response->status_code,.retryable=http_retryable(response->status_code)});
        if(const auto it=response->header_fields.find("grpc-status");it!=response->header_fields.end()&&it->second!="0"){const auto message_it=response->header_fields.find("grpc-message");co_return std::unexpected(error{.domain=error_domain::protocol,.code=616,.message=message_it==response->header_fields.end()?"gRPC call failed":message_it->second,.upstream_code=it->second});}
        auto payload=grpc_unframe(response->body);if(!payload)co_return std::unexpected(payload.error());co_return protocol::rpc_frame{.protobuf_payload=std::move(*payload),.end_of_stream=true};
    }
private:http_transport_ptr http_{};
};

} // namespace

http_transport_ptr make_curl_http_transport(curl_options options){auto context=std::make_shared<curl_http_context>();context->options=std::move(options);return make_http_transport(std::move(context),&curl_http_send);}
protocol::transport_ptr make_curl_grpc_transport(http_transport_ptr http,curl_options options){if(!http)http=make_curl_http_transport(std::move(options));return std::make_shared<curl_grpc_transport>(std::move(http));}

} // namespace quilibrium::transport
