module;
#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <expected>
#include <iomanip>
#include <map>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <set>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module quilibrium.sigv4;

namespace quilibrium::auth {
namespace {

constexpr std::chrono::seconds max_presign_expiration{604800};
constexpr std::string_view algorithm{"AWS4-HMAC-SHA256"};
constexpr std::string_view unsigned_payload{"UNSIGNED-PAYLOAD"};

[[nodiscard]] std::int32_t error_code(sigv4_error_code value) noexcept {
    return static_cast<std::int32_t>(value);
}

[[nodiscard]] error make_error(error_domain domain, sigv4_error_code code, std::string message) {
    return error{.domain = domain, .code = error_code(code), .message = std::move(message), .retryable = false};
}

[[nodiscard]] std::string sha256_hex(byte_view input) {
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int length = 0;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (ctx == nullptr) return {};
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx, input.data(), input.size()) != 1 ||
        EVP_DigestFinal_ex(ctx, digest.data(), &length) != 1) {
        EVP_MD_CTX_free(ctx);
        return {};
    }
    EVP_MD_CTX_free(ctx);
    return quilibrium::hex({reinterpret_cast<const byte*>(digest.data()), length});
}

[[nodiscard]] std::vector<unsigned char> hmac_sha256(
    std::span<const unsigned char> key,
    std::string_view data) {
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int length = 0;
    HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(), digest.data(), &length);
    return {digest.begin(), digest.begin() + static_cast<std::ptrdiff_t>(length)};
}

[[nodiscard]] std::vector<unsigned char> hmac_sha256(std::string_view key, std::string_view data) {
    return hmac_sha256({reinterpret_cast<const unsigned char*>(key.data()), key.size()}, data);
}

[[nodiscard]] std::string trim_collapse(std::string value) {
    const auto not_space = [](unsigned char c) { return std::isspace(c) == 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](char ch) {
        return not_space(static_cast<unsigned char>(ch));
    }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](char ch) {
        return not_space(static_cast<unsigned char>(ch));
    }).base(), value.end());

    std::string out;
    bool previous_space = false;
    for (const char ch : value) {
        const auto c = static_cast<unsigned char>(ch);
        if (std::isspace(c) != 0) {
            if (!previous_space) out.push_back(' ');
            previous_space = true;
        } else {
            out.push_back(ch);
            previous_space = false;
        }
    }
    return out;
}

[[nodiscard]] std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](char ch) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    });
    return value;
}

[[nodiscard]] std::pair<std::string, std::string> utc_stamp(std::chrono::system_clock::time_point now) {
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &time);
#else
    gmtime_r(&time, &utc);
#endif
    std::ostringstream full;
    full << std::put_time(&utc, "%Y%m%dT%H%M%SZ");
    std::ostringstream date;
    date << std::put_time(&utc, "%Y%m%d");
    return {full.str(), date.str()};
}

[[nodiscard]] bool is_unreserved(unsigned char c) noexcept {
    return (c >= static_cast<unsigned char>('A') && c <= static_cast<unsigned char>('Z')) ||
           (c >= static_cast<unsigned char>('a') && c <= static_cast<unsigned char>('z')) ||
           (c >= static_cast<unsigned char>('0') && c <= static_cast<unsigned char>('9')) ||
           c == static_cast<unsigned char>('-') || c == static_cast<unsigned char>('_') ||
           c == static_cast<unsigned char>('.') || c == static_cast<unsigned char>('~');
}

[[nodiscard]] bool is_hex(char c) noexcept {
    const auto value = static_cast<unsigned char>(c);
    return (value >= static_cast<unsigned char>('0') && value <= static_cast<unsigned char>('9')) ||
           (value >= static_cast<unsigned char>('a') && value <= static_cast<unsigned char>('f')) ||
           (value >= static_cast<unsigned char>('A') && value <= static_cast<unsigned char>('F'));
}

[[nodiscard]] char upper_hex(char c) noexcept {
    const auto value = static_cast<unsigned char>(c);
    if (value >= static_cast<unsigned char>('a') && value <= static_cast<unsigned char>('f')) {
        return static_cast<char>(value - static_cast<unsigned char>('a') + static_cast<unsigned char>('A'));
    }
    return c;
}

/**
 * Canonicalizes a URI component that may already contain valid %XX escapes.
 * Existing escapes are retained (with uppercase hex) so presigning never
 * double-encodes targets produced by the SDK's percent_encode().
 */
[[nodiscard]] std::string canonicalize_encoded_component(std::string_view input, bool preserve_slash) {
    static constexpr char digits[] = "0123456789ABCDEF";
    std::string out;
    for (std::size_t i = 0; i < input.size(); ++i) {
        const char ch = input[i];
        if (ch == '%' && i + 2U < input.size() && is_hex(input[i + 1U]) && is_hex(input[i + 2U])) {
            out.push_back('%');
            out.push_back(upper_hex(input[i + 1U]));
            out.push_back(upper_hex(input[i + 2U]));
            i += 2U;
            continue;
        }
        const auto c = static_cast<unsigned char>(ch);
        if (is_unreserved(c) || (preserve_slash && c == static_cast<unsigned char>('/'))) {
            out.push_back(ch);
        } else {
            out.push_back('%');
            out.push_back(digits[(c >> 4U) & 0xFU]);
            out.push_back(digits[c & 0xFU]);
        }
    }
    return out;
}

struct parsed_target final {
    std::string path{};
    std::string query{};
};

[[nodiscard]] parsed_target split_target(std::string_view target) {
    const auto query_position = target.find('?');
    if (query_position == std::string_view::npos) {
        return {.path = std::string(target), .query = {}};
    }
    return {
        .path = std::string(target.substr(0, query_position)),
        .query = std::string(target.substr(query_position + 1U))
    };
}

[[nodiscard]] std::string canonical_uri(const http_request& request, std::string_view target_path) {
    std::string absolute_path = request.target_endpoint.base_path;
    absolute_path += target_path;
    if (absolute_path.empty()) absolute_path = "/";
    if (!absolute_path.starts_with('/')) absolute_path.insert(absolute_path.begin(), '/');
    return canonicalize_encoded_component(absolute_path, true);
}

struct query_parameter final {
    std::string name{};
    std::string value{};
};

void append_canonical_encoded(std::string& out, std::string_view input, bool preserve_slash) {
    static constexpr char digits[] = "0123456789ABCDEF";
    for (std::size_t i = 0; i < input.size(); ++i) {
        const char ch = input[i];
        if (ch == '%' && i + 2U < input.size() && is_hex(input[i + 1U]) && is_hex(input[i + 2U])) {
            out.push_back('%');
            out.push_back(upper_hex(input[i + 1U]));
            out.push_back(upper_hex(input[i + 2U]));
            i += 2U;
            continue;
        }
        const auto c = static_cast<unsigned char>(ch);
        if (is_unreserved(c) || (preserve_slash && c == static_cast<unsigned char>('/'))) {
            out.push_back(ch);
        } else {
            out.push_back('%');
            out.push_back(digits[(c >> 4U) & 0xFU]);
            out.push_back(digits[c & 0xFU]);
        }
    }
}

void append_raw_encoded(std::string& out, std::string_view input, bool preserve_slash) {
    static constexpr char digits[] = "0123456789ABCDEF";
    for (const char ch : input) {
        const auto c = static_cast<unsigned char>(ch);
        if (is_unreserved(c) || (preserve_slash && c == static_cast<unsigned char>('/'))) {
            out.push_back(ch);
        } else {
            out.push_back('%');
            out.push_back(digits[(c >> 4U) & 0xFU]);
            out.push_back(digits[c & 0xFU]);
        }
    }
}

[[nodiscard]] std::vector<query_parameter> parse_existing_query(std::string_view query) {
    std::size_t count = query.empty() ? 0U : 1U;
    count += static_cast<std::size_t>(std::count(query.begin(), query.end(), '&'));

    std::vector<query_parameter> parameters;
    // Reserve enough room for all existing parameters plus the seven SigV4
    // query-auth fields. This also avoids unnecessary string moves in older
    // GCC Modules TS implementations.
    parameters.reserve(count + 7U);
    if (query.empty()) return parameters;

    std::size_t offset = 0;
    while (offset <= query.size()) {
        const auto ampersand = query.find('&', offset);
        const auto segment = query.substr(
            offset,
            ampersand == std::string_view::npos ? query.size() - offset : ampersand - offset);
        const auto equals = segment.find('=');
        const auto name = equals == std::string_view::npos ? segment : segment.substr(0, equals);
        const auto value = equals == std::string_view::npos ? std::string_view{} : segment.substr(equals + 1U);

        parameters.emplace_back();
        auto& parameter = parameters.back();
        append_canonical_encoded(parameter.name, name, false);
        append_canonical_encoded(parameter.value, value, false);

        if (ampersand == std::string_view::npos) break;
        offset = ampersand + 1U;
    }
    return parameters;
}

void append_raw_query_parameter(
    std::vector<query_parameter>& parameters,
    std::string_view name,
    std::string_view value) {
    parameters.emplace_back();
    auto& parameter = parameters.back();
    parameter.name.assign(name);
    append_raw_encoded(parameter.value, value, false);
}

void append_encoded_query_parameter(
    std::vector<query_parameter>& parameters,
    std::string_view name,
    std::string value) {
    parameters.emplace_back();
    auto& parameter = parameters.back();
    parameter.name.assign(name);
    parameter.value.swap(value);
}

[[nodiscard]] std::string canonical_query(const std::vector<query_parameter>& parameters) {
    std::vector<const query_parameter*> sorted;
    sorted.reserve(parameters.size());
    for (const auto& parameter : parameters) sorted.push_back(&parameter);
    std::sort(sorted.begin(), sorted.end(), [](const auto* lhs, const auto* rhs) {
        if (lhs->name != rhs->name) return lhs->name < rhs->name;
        return lhs->value < rhs->value;
    });

    std::string out;
    for (const auto* parameter : sorted) {
        if (!out.empty()) out.push_back('&');
        out.append(parameter->name);
        out.push_back('=');
        out.append(parameter->value);
    }
    return out;
}

[[nodiscard]] bool is_reserved_presign_parameter(std::string_view encoded_name) {
    // Generated SigV4 names use only ASCII unreserved bytes, so a simple
    // case-insensitive comparison is sufficient and avoids decoding arbitrary
    // user query data.
    const auto name = lower(std::string(encoded_name));
    return name == "x-amz-algorithm" || name == "x-amz-credential" ||
           name == "x-amz-date" || name == "x-amz-expires" ||
           name == "x-amz-signedheaders" || name == "x-amz-signature" ||
           name == "x-amz-security-token";
}

[[nodiscard]] result<std::map<std::string, std::string>> normalized_headers(const http_headers& headers) {
    std::map<std::string, std::string> normalized;
    for (const auto& [name, value] : headers) {
        const auto canonical_name = lower(name);
        const auto canonical_value = trim_collapse(value);
        if (auto it = normalized.find(canonical_name); it != normalized.end()) {
            it->second += ',';
            it->second += canonical_value;
        } else {
            normalized.emplace(canonical_name, canonical_value);
        }
    }
    return normalized;
}

struct canonical_header_set final {
    std::string block{};
    std::string names{};
    http_headers required{};
};

[[nodiscard]] result<canonical_header_set> build_presigned_headers(
    const http_request& request,
    const presign_options& options) {
    auto normalized_result = normalized_headers(request.header_fields);
    if (!normalized_result) return std::unexpected(normalized_result.error());
    auto normalized = std::move(*normalized_result);
    normalized["host"] = request.target_endpoint.authority();

    std::set<std::string> names{"host"};
    for (const auto& [name, unused] : normalized) {
        (void)unused;
        if (name.starts_with("x-amz-")) names.insert(name);
    }

    for (const auto& requested_name : options.signed_headers) {
        const auto canonical_name = lower(requested_name);
        if (canonical_name == "host") {
            names.insert("host");
            continue;
        }
        if (!normalized.contains(canonical_name)) {
            std::string message{"SigV4 presigning requested a header that is not present: "};
            message.append(canonical_name);
            return std::unexpected(make_error(
                error_domain::validation,
                sigv4_error_code::missing_signed_header,
                std::move(message)));
        }
        names.insert(canonical_name);
    }

    canonical_header_set result{};
    for (const auto& name : names) {
        const auto it = normalized.find(name);
        if (it == normalized.end()) {
            std::string message{"SigV4 signed header is unavailable: "};
            message.append(name);
            return std::unexpected(make_error(
                error_domain::validation,
                sigv4_error_code::missing_signed_header,
                std::move(message)));
        }
        result.block += name;
        result.block.push_back(':');
        result.block += it->second;
        result.block.push_back('\n');
        if (!result.names.empty()) result.names.push_back(';');
        result.names += name;
        if (name != "host") result.required.emplace(name, it->second);
    }
    return result;
}


[[nodiscard]] std::string credential_scope(
    std::string_view date_stamp,
    std::string_view region,
    std::string_view service) {
    std::string out;
    out.append(date_stamp);
    out.push_back('/');
    out.append(region);
    out.push_back('/');
    out.append(service);
    out.append("/aws4_request");
    return out;
}

[[nodiscard]] std::string credential_value(
    std::string_view access_key_id,
    std::string_view scope) {
    std::string out;
    out.append(access_key_id);
    out.push_back('/');
    out.append(scope);
    return out;
}

[[nodiscard]] std::string canonical_request_text(
    http_method method,
    std::string_view path,
    std::string_view query,
    std::string_view headers_block,
    std::string_view signed_headers,
    std::string_view payload_hash) {
    std::string out;
    out.append(http_method_name(method));
    out.push_back('\n');
    out.append(path);
    out.push_back('\n');
    out.append(query);
    out.push_back('\n');
    out.append(headers_block);
    out.push_back('\n');
    out.append(signed_headers);
    out.push_back('\n');
    out.append(payload_hash);
    return out;
}

[[nodiscard]] std::string string_to_sign_text(
    std::string_view amz_date,
    std::string_view scope,
    std::string_view canonical_request_hash) {
    std::string out;
    out.append(algorithm);
    out.push_back('\n');
    out.append(amz_date);
    out.push_back('\n');
    out.append(scope);
    out.push_back('\n');
    out.append(canonical_request_hash);
    return out;
}

[[nodiscard]] std::vector<unsigned char> derive_signing_key(
    std::string_view secret_access_key,
    std::string_view date_stamp,
    std::string_view region,
    std::string_view service) {
    std::string initial_key{"AWS4"};
    initial_key.append(secret_access_key);
    auto k_date = hmac_sha256(initial_key, date_stamp);
    auto k_region = hmac_sha256(k_date, region);
    auto k_service = hmac_sha256(k_region, service);
    return hmac_sha256(k_service, "aws4_request");
}

[[nodiscard]] std::string signature_hex(
    std::string_view secret_access_key,
    std::string_view date_stamp,
    std::string_view region,
    std::string_view service,
    std::string_view string_to_sign) {
    auto signing_key = derive_signing_key(secret_access_key, date_stamp, region, service);
    auto signature = hmac_sha256(signing_key, string_to_sign);
    return quilibrium::hex({reinterpret_cast<const byte*>(signature.data()), signature.size()});
}

} // namespace

sigv4_signer::sigv4_signer(sigv4_credentials credentials, std::string region, std::string service)
    : credentials_(std::move(credentials)), region_(std::move(region)), service_(std::move(service)) {}

status sigv4_signer::sign(http_request& request, std::chrono::system_clock::time_point now) const {
    if (credentials_.access_key_id.empty() || credentials_.secret_access_key.empty()) {
        return std::unexpected(make_error(
            error_domain::authentication,
            sigv4_error_code::incomplete_credentials,
            "SigV4 credentials are incomplete"));
    }

    const auto [amz_date, date_stamp] = utc_stamp(now);
    request.header_fields["host"] = request.target_endpoint.authority();
    request.header_fields["x-amz-date"] = amz_date;
    if (!credentials_.session_token.empty()) {
        request.header_fields["x-amz-security-token"] = credentials_.session_token;
    }

    const std::string payload_hash = sha256_hex(request.body);
    request.header_fields["x-amz-content-sha256"] = payload_hash;

    auto canonical_headers_result = normalized_headers(request.header_fields);
    if (!canonical_headers_result) return std::unexpected(canonical_headers_result.error());

    std::string headers_block;
    std::string signed_headers;
    for (const auto& [name, value] : *canonical_headers_result) {
        headers_block.append(name);
        headers_block.push_back(':');
        headers_block.append(value);
        headers_block.push_back('\n');
        if (!signed_headers.empty()) signed_headers.push_back(';');
        signed_headers.append(name);
    }

    const auto target = split_target(request.target);
    const auto path = canonical_uri(request, target.path);
    const auto query = canonical_query(parse_existing_query(target.query));

    const std::string canonical_request = canonical_request_text(
        request.verb, path, query, headers_block, signed_headers, payload_hash);
    const std::string scope = credential_scope(date_stamp, region_, service_);
    const std::string string_to_sign = string_to_sign_text(
        amz_date, scope, sha256_hex(as_bytes(canonical_request)));
    const auto signature = signature_hex(credentials_.secret_access_key, date_stamp, region_, service_, string_to_sign);

    std::string authorization;
    authorization.append(algorithm);
    authorization.append(" Credential=");
    authorization.append(credentials_.access_key_id);
    authorization.push_back('/');
    authorization.append(scope);
    authorization.append(", SignedHeaders=");
    authorization.append(signed_headers);
    authorization.append(", Signature=");
    authorization.append(signature);
    request.header_fields["authorization"] = std::move(authorization);
    return {};
}

result<presigned_request> sigv4_signer::presign(
    http_request request,
    presign_options options,
    std::chrono::system_clock::time_point now) const {
    if (credentials_.access_key_id.empty() || credentials_.secret_access_key.empty()) {
        return std::unexpected(make_error(
            error_domain::authentication,
            sigv4_error_code::incomplete_credentials,
            "SigV4 credentials are incomplete"));
    }
    if (options.expires <= std::chrono::seconds::zero() || options.expires > max_presign_expiration) {
        return std::unexpected(make_error(
            error_domain::validation,
            sigv4_error_code::invalid_expiration,
            "SigV4 presigned URL expiration must be between 1 and 604800 seconds"));
    }
    if (request.target_endpoint.scheme != "https" && request.target_endpoint.scheme != "http") {
        return std::unexpected(make_error(
            error_domain::validation,
            sigv4_error_code::unsupported_endpoint_scheme,
            "SigV4 presigning requires an http or https endpoint"));
    }

    const auto target = split_target(request.target);
    auto parameters = parse_existing_query(target.query);
    for (const auto& parameter : parameters) {
        if (is_reserved_presign_parameter(parameter.name)) {
            return std::unexpected(make_error(
                error_domain::validation,
                sigv4_error_code::conflicting_query_parameter,
                "request target already contains a SigV4 presigning query parameter"));
        }
    }

    const auto headers = build_presigned_headers(request, options);
    if (!headers) return std::unexpected(headers.error());

    const auto [amz_date, date_stamp] = utc_stamp(now);
    const std::string scope = credential_scope(date_stamp, region_, service_);
    const std::string credential = credential_value(credentials_.access_key_id, scope);

    append_raw_query_parameter(parameters, "X-Amz-Algorithm", algorithm);
    append_raw_query_parameter(parameters, "X-Amz-Credential", credential);
    append_raw_query_parameter(parameters, "X-Amz-Date", amz_date);
    append_encoded_query_parameter(parameters, "X-Amz-Expires", std::to_string(options.expires.count()));
    append_raw_query_parameter(parameters, "X-Amz-SignedHeaders", headers->names);
    if (!credentials_.session_token.empty()) {
        append_raw_query_parameter(parameters, "X-Amz-Security-Token", credentials_.session_token);
    }

    const auto query = canonical_query(parameters);
    const auto path = canonical_uri(request, target.path);
    const auto payload_hash = options.payload_mode == presign_payload_mode::unsigned_payload
                                  ? std::string(unsigned_payload)
                                  : sha256_hex(request.body);

    const std::string canonical_request = canonical_request_text(
        request.verb, path, query, headers->block, headers->names, payload_hash);
    const std::string string_to_sign = string_to_sign_text(
        amz_date, scope, sha256_hex(as_bytes(canonical_request)));
    const auto signature = signature_hex(credentials_.secret_access_key, date_stamp, region_, service_, string_to_sign);

    std::string url = request.target_endpoint.origin();
    url += path;
    url.push_back('?');
    url += query;
    url += "&X-Amz-Signature=";
    url += signature;

    return presigned_request{
        .url = std::move(url),
        .required_headers = headers->required,
        .expires_at = now + options.expires
    };
}

} // namespace quilibrium::auth
