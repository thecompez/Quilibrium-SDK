module;
#include <expected>
#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstddef>
#include <ctime>
#include <iomanip>
#include <map>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

module quilibrium.sigv4;

namespace quilibrium::auth {
namespace {

std::string sha256_hex(byte_view input) {
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int length = 0;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx, input.data(), input.size());
    EVP_DigestFinal_ex(ctx, digest.data(), &length);
    EVP_MD_CTX_free(ctx);
    return quilibrium::hex({reinterpret_cast<const byte*>(digest.data()), length});
}

std::vector<unsigned char> hmac_sha256(std::span<const unsigned char> key, std::string_view data) {
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int length = 0;
    HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(), digest.data(), &length);
    return {digest.begin(), digest.begin() + length};
}

std::vector<unsigned char> hmac_sha256(std::string_view key, std::string_view data) {
    return hmac_sha256({reinterpret_cast<const unsigned char*>(key.data()), key.size()}, data);
}

std::string trim_collapse(std::string value) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    std::string out;
    bool previous_space = false;
    for (const char ch : value) {
        const auto c = static_cast<unsigned char>(ch);
        if (std::isspace(c)) {
            if (!previous_space) out.push_back(' ');
            previous_space = true;
        } else {
            out.push_back(static_cast<char>(c));
            previous_space = false;
        }
    }
    return out;
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::pair<std::string, std::string> utc_stamp(std::chrono::system_clock::time_point now) {
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::ostringstream full;
    full << std::put_time(&tm, "%Y%m%dT%H%M%SZ");
    std::ostringstream date;
    date << std::put_time(&tm, "%Y%m%d");
    return {full.str(), date.str()};
}

} // namespace

sigv4_signer::sigv4_signer(sigv4_credentials credentials, std::string region, std::string service)
    : credentials_(std::move(credentials)), region_(std::move(region)), service_(std::move(service)) {}

status sigv4_signer::sign(http_request& request, std::chrono::system_clock::time_point now) const {
    if (credentials_.access_key_id.empty() || credentials_.secret_access_key.empty()) {
        return std::unexpected(error{.domain=error_domain::authentication, .code=100, .message="SigV4 credentials are incomplete"});
    }
    const auto [amz_date, date_stamp] = utc_stamp(now);
    request.header_fields["host"] = request.target_endpoint.authority();
    request.header_fields["x-amz-date"] = amz_date;
    if (!credentials_.session_token.empty()) request.header_fields["x-amz-security-token"] = credentials_.session_token;

    const std::string payload_hash = sha256_hex(request.body);
    request.header_fields["x-amz-content-sha256"] = payload_hash;

    std::map<std::string, std::string> canonical_headers;
    for (const auto& [k, v] : request.header_fields) canonical_headers[lower(k)] = trim_collapse(v);

    std::string headers_block;
    std::string signed_headers;
    for (const auto& [k, v] : canonical_headers) {
        headers_block += k + ":" + v + "\n";
        if (!signed_headers.empty()) signed_headers += ';';
        signed_headers += k;
    }

    std::string path = request.target;
    std::string query;
    if (const auto pos = path.find('?'); pos != std::string::npos) {
        query = path.substr(pos + 1);
        path.resize(pos);
    }
    if (path.empty()) path = "/";

    const std::string canonical_request = std::string(http_method_name(request.verb)) + "\n" + path + "\n" + query + "\n" +
                                          headers_block + "\n" + signed_headers + "\n" + payload_hash;
    const std::string scope = date_stamp + "/" + region_ + "/" + service_ + "/aws4_request";
    const std::string string_to_sign = "AWS4-HMAC-SHA256\n" + amz_date + "\n" + scope + "\n" + sha256_hex(as_bytes(canonical_request));

    auto k_date = hmac_sha256("AWS4" + credentials_.secret_access_key, date_stamp);
    auto k_region = hmac_sha256(k_date, region_);
    auto k_service = hmac_sha256(k_region, service_);
    auto k_signing = hmac_sha256(k_service, "aws4_request");
    auto signature = hmac_sha256(k_signing, string_to_sign);
    const std::string signature_hex = quilibrium::hex({reinterpret_cast<const byte*>(signature.data()), signature.size()});

    request.header_fields["authorization"] = "AWS4-HMAC-SHA256 Credential=" + credentials_.access_key_id + "/" + scope +
                                              ", SignedHeaders=" + signed_headers + ", Signature=" + signature_hex;
    return {};
}

} // namespace quilibrium::auth
