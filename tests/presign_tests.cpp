#include <cassert>
#include <chrono>
#include <cstddef>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

import quilibrium.core;
import quilibrium.sigv4;
import quilibrium.qstorage;
import quilibrium.sdk;

namespace {

using namespace std::chrono;

[[nodiscard]] system_clock::time_point aws_example_time() {
    // 2013-05-24T00:00:00Z from the official AWS S3 presigning example.
    // Epoch seconds avoid a GCC 14 Modules TS/O3 optimizer ICE involving
    // chrono calendar COMDATs while preserving the exact deterministic time.
    return system_clock::time_point{seconds{1'369'353'600}};
}

[[nodiscard]] bool contains(std::string_view haystack, std::string_view needle) {
    return haystack.find(needle) != std::string_view::npos;
}

} // namespace

int main() {
    const quilibrium::auth::sigv4_credentials aws_credentials{
        .access_key_id="AKIAIOSFODNN7EXAMPLE",
        .secret_access_key="wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY"
    };
    const quilibrium::auth::sigv4_signer aws_signer{aws_credentials,"us-east-1","s3"};

    // Official AWS S3 SigV4 presigning test vector:
    // https://docs.aws.amazon.com/AmazonS3/latest/API/sigv4-query-string-auth.html
    const auto official = aws_signer.presign(
        quilibrium::http_request{
            .verb=quilibrium::http_method::get,
            .target_endpoint={.scheme="https",.host="examplebucket.s3.amazonaws.com",.port=443},
            .target="/test.txt"
        },
        quilibrium::auth::presign_options{.expires=seconds{86400}},
        aws_example_time());
    assert(official);
    assert(official->required_headers.empty());
    assert(official->expires_at == aws_example_time() + seconds{86400});
    assert(official->url ==
        "https://examplebucket.s3.amazonaws.com/test.txt?"
        "X-Amz-Algorithm=AWS4-HMAC-SHA256&"
        "X-Amz-Credential=AKIAIOSFODNN7EXAMPLE%2F20130524%2Fus-east-1%2Fs3%2Faws4_request&"
        "X-Amz-Date=20130524T000000Z&"
        "X-Amz-Expires=86400&"
        "X-Amz-SignedHeaders=host&"
        "X-Amz-Signature=aeeed9bbccd4d02ee5c0109b86d86835f995330da4c265957d157751f604d404");

    // Existing parameters are preserved, encoded exactly once, duplicate keys
    // are retained, values are sorted, and empty subresources remain signable.
    const auto query = aws_signer.presign(
        quilibrium::http_request{
            .verb=quilibrium::http_method::get,
            .target_endpoint={.scheme="https",.host="s3.example.test",.port=443},
            .target="/bucket/key?z=last&a=2&uploads&a=1&space=hello%20world"
        },
        quilibrium::auth::presign_options{.expires=seconds{60}},
        aws_example_time());
    assert(query);
    assert(contains(query->url,"a=1&a=2&space=hello%20world&uploads=&z=last"));
    assert(!contains(query->url,"hello%2520world"));

    // Session credentials belong in the canonical query, never in required headers.
    const quilibrium::auth::sigv4_signer session_signer{
        {.access_key_id="SESSIONID",.secret_access_key="SESSIONSECRET",.session_token="token/with + chars"},
        "test-region",
        "s3"};
    const auto session = session_signer.presign(
        quilibrium::http_request{
            .verb=quilibrium::http_method::head,
            .target_endpoint={.scheme="https",.host="storage.example",.port=443},
            .target="/bucket/object"
        },
        {.expires=seconds{120}},
        aws_example_time());
    assert(session);
    assert(contains(session->url,"X-Amz-Security-Token=token%2Fwith%20%2B%20chars"));
    assert(!contains(session->url,"SESSIONSECRET"));

    // PUT with constrained Content-Type makes that header an explicit contract.
    const auto put_jpeg = aws_signer.presign(
        quilibrium::http_request{
            .verb=quilibrium::http_method::put,
            .target_endpoint={.scheme="https",.host="s3.example.test",.port=443},
            .target="/bucket/image.jpg",
            .header_fields={{"Content-Type","image/jpeg"}}
        },
        {.expires=minutes{15},.signed_headers={"content-type"}},
        aws_example_time());
    assert(put_jpeg);
    assert(put_jpeg->required_headers.at("content-type") == "image/jpeg");
    assert(contains(put_jpeg->url,"X-Amz-SignedHeaders=content-type%3Bhost"));

    const auto put_png = aws_signer.presign(
        quilibrium::http_request{
            .verb=quilibrium::http_method::put,
            .target_endpoint={.scheme="https",.host="s3.example.test",.port=443},
            .target="/bucket/image.jpg",
            .header_fields={{"content-type","image/png"}}
        },
        {.expires=minutes{15},.signed_headers={"content-type"}},
        aws_example_time());
    assert(put_png);
    assert(put_jpeg->url != put_png->url);

    // Missing requested signed headers are rejected rather than producing a URL
    // that later fails with SignatureDoesNotMatch.
    const auto missing_header = aws_signer.presign(
        quilibrium::http_request{
            .verb=quilibrium::http_method::put,
            .target_endpoint={.scheme="https",.host="s3.example.test",.port=443},
            .target="/bucket/object"
        },
        {.expires=minutes{15},.signed_headers={"content-type"}},
        aws_example_time());
    assert(!missing_header);
    assert(missing_header.error().domain == quilibrium::error_domain::validation);
    assert(missing_header.error().code == static_cast<std::int32_t>(quilibrium::auth::sigv4_error_code::missing_signed_header));

    // Invalid expiration values are never clamped.
    for (const auto invalid : std::vector<seconds>{seconds{0},seconds{-1},seconds{604801}}) {
        const auto result = aws_signer.presign(
            quilibrium::http_request{
                .verb=quilibrium::http_method::get,
                .target_endpoint={.scheme="https",.host="s3.example.test",.port=443},
                .target="/bucket/object"
            },
            {.expires=invalid},
            aws_example_time());
        assert(!result);
        assert(result.error().code == static_cast<std::int32_t>(quilibrium::auth::sigv4_error_code::invalid_expiration));
    }

    // Missing credentials fail without leaking secret material.
    const quilibrium::auth::sigv4_signer empty_signer{{},"region","s3"};
    const auto missing_credentials = empty_signer.presign(
        quilibrium::http_request{
            .verb=quilibrium::http_method::get,
            .target_endpoint={.scheme="https",.host="s3.example.test",.port=443},
            .target="/"
        },{},aws_example_time());
    assert(!missing_credentials);
    assert(missing_credentials.error().domain == quilibrium::error_domain::authentication);

    // QStorage helpers preserve UTF-8, spaces and reserved object-key bytes.
    const std::vector<quilibrium::endpoint> endpoints{
        {.scheme="https",.host="first.storage.example",.port=443},
        {.scheme="https",.host="second.storage.example",.port=443}
    };
    quilibrium::qstorage::client storage{
        {.endpoints=endpoints,.auth={.access_key_id="AKID",.secret_access_key="SUPERSECRET"},.region="custom-1"}};
    const std::string key = "dir/a b/\xE2\x98\x83?#.jpg";
    const auto storage_put = storage.presign_put_object("media-bucket",key,"image/jpeg",minutes{15},aws_example_time());
    assert(storage_put);
    assert(storage_put->url.starts_with("https://first.storage.example/media-bucket/"));
    assert(contains(storage_put->url,"dir/a%20b/%E2%98%83%3F%23.jpg"));
    assert(contains(storage_put->url,"%2Fcustom-1%2Fs3%2Faws4_request"));
    assert(storage_put->required_headers.at("content-type") == "image/jpeg");
    assert(!contains(storage_put->url,"SUPERSECRET"));

    const auto storage_get = storage.presign_get_object("media-bucket","file.txt",minutes{5},aws_example_time());
    const auto storage_head = storage.presign_head_object("media-bucket","file.txt",minutes{5},aws_example_time());
    assert(storage_get && storage_head);
    assert(storage_get->url != storage_head->url);

    // Multipart-compatible arbitrary query targets can be presigned without a
    // dedicated API redesign.
    const auto upload_part = storage.presign(
        quilibrium::http_method::put,
        "/media-bucket/large.bin?uploadId=abc%2F123&partNumber=1",
        {},
        {.expires=minutes{10}},
        aws_example_time());
    assert(upload_part);
    assert(contains(upload_part->url,"partNumber=1&uploadId=abc%2F123"));
    assert(!contains(upload_part->url,"abc%252F123"));

    // Facade exposes the same capability and an explicitly configurable region.
    quilibrium::sdk_config config{};
    config.qstorage_endpoints={{.scheme="https",.host="facade.storage.example",.port=443}};
    config.qstorage_credentials=quilibrium::sdk_credentials{.access_key_id="FACADEID",.secret_access_key="FACADESECRET"};
    config.qstorage_region="facade-region-1";
    // No network I/O occurs while presigning; a transport is only required by connect().
    struct noop_context final {};
    auto context=std::make_shared<noop_context>();
    config.http=quilibrium::make_http_transport(context,[](void*,quilibrium::http_request,quilibrium::call_options) -> quilibrium::result<quilibrium::http_response> {
        return std::unexpected(quilibrium::error{.domain=quilibrium::error_domain::transport,.code=1,.message="not used"});
    });
    auto sdk=quilibrium::connect(std::move(config));
    assert(sdk);
    const auto facade_put=sdk->storage().presign_put("bucket","image.jpg","image/jpeg",minutes{15});
    const auto facade_get=sdk->storage().presign_get("bucket","image.jpg",minutes{15});
    const auto facade_head=sdk->storage().presign_head("bucket","image.jpg",minutes{15});
    assert(facade_put && facade_get && facade_head);
    assert(contains(facade_put->url,"%2Ffacade-region-1%2Fs3%2Faws4_request"));
    assert(facade_put->required_headers.at("content-type") == "image/jpeg");
    assert(!contains(facade_put->url,"FACADESECRET"));

    return 0;
}
