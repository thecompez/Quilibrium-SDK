#include <cassert>
#include <coroutine>
#include <memory>
#include <string>
#include <vector>

import quilibrium.core;
import quilibrium.hypersnap;
import quilibrium.qstorage;
import quilibrium.qkms;
import quilibrium.protocol;

struct mock_state final {
    bool saw_authorization{false};
    bool saw_kms_target{false};
    bool saw_multipart{false};
    bool saw_rotation_target{false};
};

quilibrium::result<quilibrium::http_response> service_send(void* opaque,quilibrium::http_request request,quilibrium::call_options) {
    auto* state=static_cast<mock_state*>(opaque);
    if (request.header_fields.contains("authorization")) state->saw_authorization=true;
    if (request.target.find("?uploads") != std::string::npos) state->saw_multipart=true;
    if (const auto it=request.header_fields.find("x-amz-target");it!=request.header_fields.end()) {
        if (it->second=="TrentService.Sign") state->saw_kms_target=true;
        if (it->second=="TrentService.EnableKeyRotation") state->saw_rotation_target=true;
    }
    std::string body=request.target.starts_with("/v2/farcaster/user")?R"({"user":{"fid":3,"username":"dwr.eth"}})":"{}";
    const auto view=quilibrium::as_bytes(body);
    return quilibrium::http_response{.status_code=200,.header_fields={{"content-type","application/json"}},.body=quilibrium::bytes(view.begin(),view.end())};
}


class mock_rpc final : public quilibrium::protocol::transport {
public:
    quilibrium::task<quilibrium::result<quilibrium::protocol::rpc_frame>> unary(
        quilibrium::endpoint,quilibrium::protocol::rpc_call call,quilibrium::call_options) override {
        co_return quilibrium::protocol::rpc_frame{.protobuf_payload=std::move(call.protobuf_payload),.end_of_stream=true};
    }
};

int main() {
    auto state=std::make_shared<mock_state>();
    auto transport=quilibrium::make_http_transport(state,&service_send);
    std::vector<quilibrium::endpoint> endpoints{{.scheme="https",.host="example.test",.port=443}};

    {
        quilibrium::hypersnap::client client({.endpoints=endpoints},transport);
        auto response=quilibrium::sync_wait(client.user_by_fid(3));
        assert(response&&response->status_code==200);
    }

    {
        quilibrium::qstorage::client client({.endpoints=endpoints,.auth={.access_key_id="AKID",.secret_access_key="SECRET"},.region="q"},transport);
        auto response=quilibrium::sync_wait(client.create_bucket("sdk-test"));
        assert(response&&response->status_code==200);
        auto multipart=quilibrium::sync_wait(client.create_multipart_upload("sdk-test","large.bin"));
        assert(multipart&&multipart->status_code==200);
        assert(state->saw_authorization);
        assert(state->saw_multipart);
    }

    {
        quilibrium::qkms::client client({.endpoints=endpoints,.auth={.access_key_id="AKID",.secret_access_key="SECRET"},.region="q"},transport);
        auto response=quilibrium::sync_wait(client.sign({}));
        assert(response&&response->status_code==200);
        auto rotation=quilibrium::sync_wait(client.enable_key_rotation({}));
        assert(rotation&&rotation->status_code==200);
        assert(state->saw_kms_target);
        assert(state->saw_rotation_target);
    }

    {
        auto rpc=std::make_shared<mock_rpc>();
        quilibrium::protocol::client client(endpoints,rpc);
        quilibrium::bytes payload{std::byte{0x01},std::byte{0x02}};
        auto response=quilibrium::sync_wait(client.call({.target_service=quilibrium::protocol::service::node,.method="GetNodeInfo",.mode=quilibrium::protocol::streaming::unary,.protobuf_payload=payload}));
        assert(response&&response->protobuf_payload==payload);
    }

    const auto ferret=quilibrium::protocol::method_registry::methods(quilibrium::protocol::service::ferret_proxy);
    assert(ferret.size()==2&&ferret[0]=="AliceProxy"&&ferret[1]=="BobProxy");
    return 0;
}
