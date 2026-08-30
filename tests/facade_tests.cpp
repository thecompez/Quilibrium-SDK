#include <cassert>
#include <cstddef>
#include <memory>
#include <string>

import quilibrium.core;
import quilibrium.sdk;

struct mock_state final {
    bool saw_storage_auth{false};
    bool saw_kms_auth{false};
    bool saw_native_grpc{false};
};

quilibrium::result<quilibrium::http_response> mock_send(void* opaque,quilibrium::http_request request,quilibrium::call_options) {
    auto* state=static_cast<mock_state*>(opaque);
    std::string body;
    quilibrium::http_headers headers{{"content-type","application/json"}};

    if(request.target.starts_with("/v2/farcaster/user?")) {
        body=R"({"user":{"fid":3,"username":"dwr.eth","display_name":"DWR","pfp_url":"https://example/p.png","profile":{"bio":{"text":"hello"}},"follower_count":10,"following_count":5}})";
    } else if(request.target.starts_with("/v2/farcaster/feed/trending")) {
        body=R"({"casts":[{"hash":"0xabc","author":{"fid":3,"username":"dwr.eth"},"text":"hello","reactions":{"likes_count":4,"recasts_count":2},"replies":{"count":1}}],"next":{"cursor":"next"}})";
    } else if(request.target=="/bucket/object.bin") {
        state->saw_storage_auth=request.header_fields.contains("authorization");
        body="storage-ok";
    } else if(request.target=="/"&&request.header_fields.contains("x-amz-target")) {
        state->saw_kms_auth=request.header_fields.contains("authorization");
        body=R"({"Signature":"AA=="})";
    } else if(request.target=="/quilibrium.node.node.pb.NodeService/GetNodeInfo") {
        state->saw_native_grpc=true;
        headers={{"content-type","application/grpc"},{"grpc-status","0"}};
        quilibrium::bytes framed{std::byte{0},std::byte{0},std::byte{0},std::byte{0},std::byte{2},std::byte{0x08},std::byte{0x01}};
        return quilibrium::http_response{.status_code=200,.header_fields=std::move(headers),.body=std::move(framed)};
    } else {
        body=R"({"casts":[]})";
    }

    const auto view=quilibrium::as_bytes(body);
    return quilibrium::http_response{.status_code=200,.header_fields=std::move(headers),.body=quilibrium::bytes(view.begin(),view.end())};
}

int main(){
    auto state=std::make_shared<mock_state>();
    quilibrium::sdk_config config{};
    config.http=quilibrium::make_http_transport(state,&mock_send);
    config.qstorage_credentials=quilibrium::sdk_credentials{.access_key_id="AKID",.secret_access_key="SECRET"};
    config.qkms_credentials=quilibrium::sdk_credentials{.access_key_id="AKID",.secret_access_key="SECRET"};
    config.protocol_endpoints={{.scheme="https",.host="node.example",.port=443}};

    auto q=quilibrium::connect(std::move(config));
    assert(q);

    auto user=quilibrium::sync_wait(q->hypersnap().users().get_by_fid(3));
    assert(user&&user->fid==3&&user->username=="dwr.eth"&&user->bio=="hello");

    auto feed=quilibrium::sync_wait(q->hypersnap().feeds().trending());
    assert(feed&&feed->casts.size()==1&&feed->casts[0].likes==4&&feed->cursor=="next");

    quilibrium::bytes object{std::byte{1},std::byte{2}};
    auto stored=quilibrium::sync_wait(q->storage().put("bucket","object.bin",std::move(object)));
    assert(stored&&stored->status_code==200&&state->saw_storage_auth);

    auto signed_value=quilibrium::sync_wait(q->kms().invoke("Sign",R"({"KeyId":"test"})"));
    assert(signed_value&&signed_value->status_code==200&&state->saw_kms_auth);

    auto node_info=quilibrium::sync_wait(q->native().call(quilibrium::native_service::node,"GetNodeInfo",{}));
    assert(node_info&&node_info->size()==2&&state->saw_native_grpc);
    assert((*node_info)[0]==std::byte{0x08}&&(*node_info)[1]==std::byte{0x01});

    return 0;
}
