#include <chrono>
#include <cstdlib>
#include <expected>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

import quilibrium;

namespace {

[[nodiscard]] const char* env(const char* name) { return std::getenv(name); }

[[nodiscard]] quilibrium::result<quilibrium::http_request> request_from_presigned(
    quilibrium::http_method method,
    const quilibrium::presigned_url& presigned,
    quilibrium::bytes body = {}) {
    const auto scheme_end=presigned.url.find("://");
    if (scheme_end==std::string::npos) {
        return std::unexpected(quilibrium::error{.domain=quilibrium::error_domain::validation,.code=1,.message="invalid presigned URL"});
    }
    const auto path_start=presigned.url.find('/',scheme_end+3U);
    const std::string origin=path_start==std::string::npos?presigned.url:presigned.url.substr(0,path_start);
    auto endpoint=quilibrium::parse_endpoint(origin);
    if (!endpoint) return std::unexpected(endpoint.error());
    const std::string target=path_start==std::string::npos?"/":presigned.url.substr(path_start);
    return quilibrium::http_request{
        .verb=method,
        .target_endpoint=std::move(*endpoint),
        .target=target,
        .header_fields=presigned.required_headers,
        .body=std::move(body)
    };
}

} // namespace

int main() {
    const char* access=env("Q_ACCESS_KEY_ID");
    const char* secret=env("Q_SECRET_ACCESS_KEY");
    const char* bucket=env("Q_STORAGE_BUCKET");
    if (access==nullptr || secret==nullptr || bucket==nullptr) {
        std::cout << "SKIP: Q_ACCESS_KEY_ID, Q_SECRET_ACCESS_KEY and Q_STORAGE_BUCKET are required\n";
        return 77;
    }

    quilibrium::sdk_config config{};
    config.qstorage_credentials=quilibrium::sdk_credentials{.access_key_id=access,.secret_access_key=secret};
    if (const char* region=env("Q_STORAGE_REGION")) config.qstorage_region=region;
    if (const char* endpoint_url=env("Q_STORAGE_ENDPOINT")) {
        auto endpoint=quilibrium::parse_endpoint(endpoint_url);
        if (!endpoint) {
            std::cerr << endpoint.error().message << '\n';
            return 1;
        }
        config.qstorage_endpoints={std::move(*endpoint)};
    }

    auto sdk=quilibrium::connect(std::move(config));
    if (!sdk) {
        std::cerr << sdk.error().message << '\n';
        return 1;
    }

    const auto suffix=std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const std::string key="quilibrium-sdk-presign-test-"+std::to_string(suffix)+".txt";
    const std::string payload="quilibrium-presigned-url-integration-test";
    const auto payload_view=quilibrium::as_bytes(payload);
    quilibrium::bytes body(payload_view.begin(),payload_view.end());

    auto put=sdk->storage().presign_put(bucket,key,"text/plain",std::chrono::minutes{5});
    if (!put) { std::cerr << put.error().message << '\n'; return 1; }
    auto put_request=request_from_presigned(quilibrium::http_method::put,*put,std::move(body));
    if (!put_request) { std::cerr << put_request.error().message << '\n'; return 1; }
    auto put_response=sdk->transport()->send_now(std::move(*put_request));
    if (!put_response) { std::cout << "SKIP: network unavailable: " << put_response.error().message << '\n'; return 77; }
    if (!quilibrium::http_success(put_response->status_code)) {
        std::cerr << "presigned PUT failed with HTTP " << put_response->status_code << '\n';
        return 1;
    }

    auto get=sdk->storage().presign_get(bucket,key,std::chrono::minutes{5});
    if (!get) { std::cerr << get.error().message << '\n'; return 1; }
    auto get_request=request_from_presigned(quilibrium::http_method::get,*get);
    if (!get_request) { std::cerr << get_request.error().message << '\n'; return 1; }
    auto get_response=sdk->transport()->send_now(std::move(*get_request));
    if (!get_response) { std::cout << "SKIP: network unavailable: " << get_response.error().message << '\n'; return 77; }
    if (!quilibrium::http_success(get_response->status_code) || quilibrium::as_string(get_response->body)!=payload) {
        std::cerr << "presigned GET verification failed\n";
        return 1;
    }

    auto head=sdk->storage().presign_head(bucket,key,std::chrono::minutes{5});
    if (!head) { std::cerr << head.error().message << '\n'; return 1; }
    auto head_request=request_from_presigned(quilibrium::http_method::head,*head);
    if (!head_request) { std::cerr << head_request.error().message << '\n'; return 1; }
    auto head_response=sdk->transport()->send_now(std::move(*head_request));
    if (!head_response) { std::cout << "SKIP: network unavailable: " << head_response.error().message << '\n'; return 77; }
    if (!quilibrium::http_success(head_response->status_code)) {
        std::cerr << "presigned HEAD failed with HTTP " << head_response->status_code << '\n';
        return 1;
    }

    const auto cleanup=quilibrium::sync_wait(sdk->storage().remove(bucket,key));
    if (!cleanup || !quilibrium::http_success(cleanup->status_code)) {
        std::cerr << "warning: test object cleanup failed\n";
    }

    std::cout << "QStorage presigned PUT/GET/HEAD integration test passed\n";
    return 0;
}
