#include "quilibrium.h"
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

import quilibrium.core;
import quilibrium.sdk;

struct ql_sdk { quilibrium::sdk value; };

namespace {
char* copy_text(std::string_view text) {
    auto* out=static_cast<char*>(std::malloc(text.size()+1));
    if(!out) return nullptr;
    std::memcpy(out,text.data(),text.size()); out[text.size()]='\0'; return out;
}
void set_error(ql_error* out,const quilibrium::error& error) {
    if(!out) return;
    out->domain=static_cast<int32_t>(error.domain); out->code=error.code;
    out->http_status=error.http_status.value_or(0); out->message=copy_text(error.message);
}
void set_response(ql_response* out,int32_t status,quilibrium::byte_view body) {
    if(!out) return;
    out->status_code=status; out->body.size=body.size(); out->body.data=nullptr;
    if(body.empty()) return;
    out->body.data=static_cast<uint8_t*>(std::malloc(body.size()));
    if(out->body.data) std::memcpy(out->body.data,body.data(),body.size()); else out->body.size=0;
}
std::string str(const char* value){return value?value:"";}
void add_endpoint(std::vector<quilibrium::endpoint>& target,const char* text) {
    if(!text||!*text) return;
    auto parsed=quilibrium::parse_endpoint(text); if(parsed) target.push_back(std::move(*parsed));
}
}

extern "C" {
const char* ql_version(void){return "1.1.0";}

ql_sdk* ql_sdk_create(const ql_sdk_config* input,ql_error* error) {
    if(error) *error={};
    quilibrium::sdk_config config{};
    if(input) {
        add_endpoint(config.hypersnap_endpoints,input->hypersnap_endpoint);
        add_endpoint(config.qstorage_endpoints,input->qstorage_endpoint);
        add_endpoint(config.qkms_endpoints,input->qkms_endpoint);
        add_endpoint(config.protocol_endpoints,input->protocol_endpoint);
        if(input->access_key_id||input->secret_access_key) {
            config.qstorage_credentials=quilibrium::sdk_credentials{.access_key_id=str(input->access_key_id),.secret_access_key=str(input->secret_access_key),.session_token=str(input->session_token)};
            config.qkms_credentials=quilibrium::sdk_credentials{.access_key_id=str(input->access_key_id),.secret_access_key=str(input->secret_access_key),.session_token=str(input->session_token)};
        }
    }
    auto created=quilibrium::connect(std::move(config));
    if(!created){set_error(error,created.error());return nullptr;}
    return new ql_sdk{.value=std::move(*created)};
}
void ql_sdk_destroy(ql_sdk* sdk){delete sdk;}

int ql_hypersnap_user_by_fid(ql_sdk* sdk,uint64_t fid,ql_response* response,ql_error* error) {
    if(!sdk) return -1; if(response)*response={}; if(error)*error={};
    auto raw=quilibrium::sync_wait(sdk->value.hypersnap().get("/v2/farcaster/user",{{"fid",std::to_string(fid)}}));
    if(!raw){set_error(error,raw.error());return -1;} set_response(response,raw->status_code,raw->body); return 0;
}
int ql_hypersnap_get(ql_sdk* sdk,const char* path,ql_response* response,ql_error* error) {
    if(!sdk||!path) return -1; if(response)*response={}; if(error)*error={};
    auto raw=quilibrium::sync_wait(sdk->value.hypersnap().get(path));
    if(!raw){set_error(error,raw.error());return -1;} set_response(response,raw->status_code,raw->body); return 0;
}
int ql_hypersnap_post(ql_sdk* sdk,const char* path,const uint8_t* data,size_t size,ql_response* response,ql_error* error) {
    if(!sdk||!path||(!data&&size)) return -1; if(response)*response={}; if(error)*error={};
    quilibrium::bytes payload(size); if(size) std::memcpy(payload.data(),data,size);
    auto raw=quilibrium::sync_wait(sdk->value.hypersnap().post(path,std::move(payload)));
    if(!raw){set_error(error,raw.error());return -1;} set_response(response,raw->status_code,raw->body); return 0;
}
int ql_qkms_invoke(ql_sdk* sdk,const char* operation,const char* payload,ql_response* response,ql_error* error) {
    if(!sdk||!operation) return -1; if(response)*response={}; if(error)*error={};
    auto raw=quilibrium::sync_wait(sdk->value.kms().invoke(operation,payload?payload:"{}"));
    if(!raw){set_error(error,raw.error());return -1;} set_response(response,raw->status_code,raw->body); return 0;
}
int ql_qstorage_put(ql_sdk* sdk,const char* bucket,const char* key,const uint8_t* data,size_t size,const char* content_type,ql_response* response,ql_error* error) {
    if(!sdk||!bucket||!key||(!data&&size)) return -1; if(response)*response={}; if(error)*error={};
    quilibrium::bytes bytes_data(size); if(size) std::memcpy(bytes_data.data(),data,size);
    auto raw=quilibrium::sync_wait(sdk->value.storage().put(bucket,key,std::move(bytes_data),content_type?content_type:"application/octet-stream"));
    if(!raw){set_error(error,raw.error());return -1;} set_response(response,raw->status_code,raw->body); return 0;
}
int ql_qstorage_get(ql_sdk* sdk,const char* bucket,const char* key,ql_response* response,ql_error* error) {
    if(!sdk||!bucket||!key) return -1; if(response)*response={}; if(error)*error={};
    auto raw=quilibrium::sync_wait(sdk->value.storage().get(bucket,key));
    if(!raw){set_error(error,raw.error());return -1;} set_response(response,raw->status_code,raw->body); return 0;
}
int ql_native_call(ql_sdk* sdk,int32_t service,const char* method,const uint8_t* data,size_t size,ql_response* response,ql_error* error) {
    if(!sdk||!method||service<0||service>11||(!data&&size)) return -1; if(response)*response={}; if(error)*error={};
    quilibrium::bytes payload(size); if(size) std::memcpy(payload.data(),data,size);
    auto raw=quilibrium::sync_wait(sdk->value.native().call(static_cast<quilibrium::native_service>(service),method,std::move(payload)));
    if(!raw){set_error(error,raw.error());return -1;} set_response(response,200,*raw); return 0;
}
void ql_response_free(ql_response* response){if(!response)return;std::free(response->body.data);*response={};}
void ql_error_free(ql_error* error){if(!error)return;std::free(error->message);*error={};}
}
