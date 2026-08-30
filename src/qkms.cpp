module;
#include <algorithm>
#include <chrono>
#include <coroutine>
#include <expected>
#include <memory>
#include <string>
#include <utility>
#include <vector>

module quilibrium.qkms;
import quilibrium.sigv4;

namespace quilibrium::qkms {
struct client::impl final {
    std::vector<endpoint> endpoints;
    auth::sigv4_signer signer;
    http_transport_ptr transport;
    std::string target_prefix;
    impl(config c,http_transport_ptr t)
        : endpoints(std::move(c.endpoints)),
          signer(auth::sigv4_credentials{.access_key_id=std::move(c.auth.access_key_id),.secret_access_key=std::move(c.auth.secret_access_key),.session_token=std::move(c.auth.session_token)},std::move(c.region),"kms"),
          transport(std::move(t)),target_prefix(std::move(c.target_prefix)) {}
};
client::client(config c,http_transport_ptr t):impl_(std::make_unique<impl>(std::move(c),std::move(t))){}
client::~client()=default;
client::client(client&&) noexcept=default;
client& client::operator=(client&&) noexcept=default;

task<result<response>> client::invoke(std::string operation,bytes payload,call_options options) {
    if(!impl_->transport) co_return std::unexpected(error{.domain=error_domain::configuration,.code=400,.message="HTTP transport is not configured"});
    if(impl_->endpoints.empty()) co_return std::unexpected(error{.domain=error_domain::configuration,.code=401,.message="QKMS endpoint list is empty"});

    const auto attempts = std::max<std::uint32_t>(1U, options.max_attempts);
    error last_error{.domain=error_domain::transport,.code=402,.message="QKMS request failed",.retryable=true};
    for(std::uint32_t attempt=0; attempt<attempts; ++attempt) {
        const auto& selected=impl_->endpoints[attempt % impl_->endpoints.size()];
        http_request request{.verb=http_method::post,.target_endpoint=selected,.target="/",.header_fields={{"content-type","application/x-amz-json-1.1"},{"x-amz-target",impl_->target_prefix+"."+operation}},.body=payload};
        if(auto status=impl_->signer.sign(request);!status) co_return std::unexpected(status.error());
        auto raw=impl_->transport->send_now(std::move(request),options);
        if(!raw) {
            last_error=raw.error();
            if(!options.allow_failover || !last_error.retryable) co_return std::unexpected(last_error);
            continue;
        }
        if(http_retryable(raw->status_code) && options.allow_failover && attempt + 1U < attempts) {
            last_error=error{.domain=error_domain::service,.code=raw->status_code,.message="QKMS returned a retryable HTTP status",.http_status=raw->status_code,.retryable=true};
            continue;
        }
        co_return response{.status_code=raw->status_code,.headers=std::move(raw->header_fields),.json=std::move(raw->body)};
    }
    co_return std::unexpected(last_error);
}

#define QL_KMS_FORWARD(name,operation) task<result<response>> client::name(bytes p,call_options o){co_return sync_wait(invoke(operation,std::move(p),o));}
QL_KMS_FORWARD(create_key,"CreateKey")
QL_KMS_FORWARD(describe_key,"DescribeKey")
QL_KMS_FORWARD(enable_key,"EnableKey")
QL_KMS_FORWARD(disable_key,"DisableKey")
QL_KMS_FORWARD(update_key_description,"UpdateKeyDescription")
QL_KMS_FORWARD(encrypt,"Encrypt")
QL_KMS_FORWARD(decrypt,"Decrypt")
QL_KMS_FORWARD(re_encrypt,"ReEncrypt")
QL_KMS_FORWARD(sign,"Sign")
QL_KMS_FORWARD(verify,"Verify")
QL_KMS_FORWARD(generate_data_key,"GenerateDataKey")
QL_KMS_FORWARD(generate_data_key_without_plaintext,"GenerateDataKeyWithoutPlaintext")
QL_KMS_FORWARD(generate_data_key_pair,"GenerateDataKeyPair")
QL_KMS_FORWARD(generate_data_key_pair_without_plaintext,"GenerateDataKeyPairWithoutPlaintext")
QL_KMS_FORWARD(generate_mac,"GenerateMac")
QL_KMS_FORWARD(verify_mac,"VerifyMac")
QL_KMS_FORWARD(get_public_key,"GetPublicKey")
QL_KMS_FORWARD(get_parameters_for_import,"GetParametersForImport")
QL_KMS_FORWARD(import_key_material,"ImportKeyMaterial")
QL_KMS_FORWARD(delete_imported_key_material,"DeleteImportedKeyMaterial")
QL_KMS_FORWARD(derive_shared_secret,"DeriveSharedSecret")
QL_KMS_FORWARD(put_key_policy,"PutKeyPolicy")
QL_KMS_FORWARD(get_key_policy,"GetKeyPolicy")
QL_KMS_FORWARD(list_key_policies,"ListKeyPolicies")
QL_KMS_FORWARD(create_alias,"CreateAlias")
QL_KMS_FORWARD(update_alias,"UpdateAlias")
QL_KMS_FORWARD(delete_alias,"DeleteAlias")
QL_KMS_FORWARD(list_aliases,"ListAliases")
QL_KMS_FORWARD(create_grant,"CreateGrant")
QL_KMS_FORWARD(retire_grant,"RetireGrant")
QL_KMS_FORWARD(revoke_grant,"RevokeGrant")
QL_KMS_FORWARD(list_grants,"ListGrants")
QL_KMS_FORWARD(list_retirable_grants,"ListRetirableGrants")
QL_KMS_FORWARD(tag_resource,"TagResource")
QL_KMS_FORWARD(untag_resource,"UntagResource")
QL_KMS_FORWARD(list_resource_tags,"ListResourceTags")
QL_KMS_FORWARD(enable_key_rotation,"EnableKeyRotation")
QL_KMS_FORWARD(disable_key_rotation,"DisableKeyRotation")
QL_KMS_FORWARD(get_key_rotation_status,"GetKeyRotationStatus")
QL_KMS_FORWARD(replicate_key,"ReplicateKey")
QL_KMS_FORWARD(update_primary_region,"UpdatePrimaryRegion")
QL_KMS_FORWARD(schedule_key_deletion,"ScheduleKeyDeletion")
QL_KMS_FORWARD(cancel_key_deletion,"CancelKeyDeletion")
#undef QL_KMS_FORWARD

task<result<response>> client::list_keys(bytes p,call_options o){co_return sync_wait(invoke("ListKeys",std::move(p),o));}
} // namespace quilibrium::qkms
