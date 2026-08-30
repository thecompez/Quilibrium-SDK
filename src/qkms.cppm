module;
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

export module quilibrium.qkms;
import quilibrium.core;

export namespace quilibrium::qkms {
struct credentials final { std::string access_key_id{}; std::string secret_access_key{}; std::string session_token{}; };
struct config final { std::vector<endpoint> endpoints{}; credentials auth{}; std::string region{"q"}; std::string target_prefix{"TrentService"}; };
struct response final { std::int32_t status_code{}; http_headers headers{}; bytes json{}; };
class client final {
public:
    client(config configuration, http_transport_ptr transport);
    ~client(); client(client&&) noexcept; client& operator=(client&&) noexcept; client(const client&)=delete; client& operator=(const client&)=delete;
    [[nodiscard]] task<result<response>> invoke(std::string operation, bytes json_payload, call_options options = {});
    [[nodiscard]] task<result<response>> create_key(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> describe_key(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> list_keys(bytes payload = {}, call_options options = {});
    [[nodiscard]] task<result<response>> enable_key(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> disable_key(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> update_key_description(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> encrypt(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> decrypt(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> re_encrypt(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> sign(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> verify(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> generate_data_key(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> generate_data_key_without_plaintext(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> generate_data_key_pair(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> generate_data_key_pair_without_plaintext(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> generate_mac(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> verify_mac(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> get_public_key(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> get_parameters_for_import(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> import_key_material(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> delete_imported_key_material(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> derive_shared_secret(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> put_key_policy(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> get_key_policy(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> list_key_policies(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> create_alias(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> update_alias(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> delete_alias(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> list_aliases(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> create_grant(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> retire_grant(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> revoke_grant(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> list_grants(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> list_retirable_grants(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> tag_resource(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> untag_resource(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> list_resource_tags(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> enable_key_rotation(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> disable_key_rotation(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> get_key_rotation_status(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> replicate_key(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> update_primary_region(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> schedule_key_deletion(bytes payload, call_options options = {});
    [[nodiscard]] task<result<response>> cancel_key_deletion(bytes payload, call_options options = {});
private: struct impl; std::unique_ptr<impl> impl_;
};
enum class threshold_algorithm : std::uint8_t { dkls23_secp256k1, frost_ed25519, feldman_bls12381, feldman_bls48581, schnorr_decaf448, frost_ed448, shoup_rsa };
} // namespace quilibrium::qkms
