module;
#include <memory>
#include <string_view>

export module quilibrium.crypto;
import quilibrium.core;

export namespace quilibrium::crypto {

/** Replaceable protocol crypto backend for algorithms not provided directly by OpenSSL. */
class protocol_provider {
public:
    virtual ~protocol_provider() = default;

    [[nodiscard]] virtual result<ed448_signature> sign_ed448(byte_view private_key, byte_view message) = 0;
    [[nodiscard]] virtual result<bool> verify_ed448(const ed448_public_key& key, byte_view message, const ed448_signature& signature) = 0;
    [[nodiscard]] virtual result<bls48581_signature> sign_bls48581(byte_view private_key, byte_view message) = 0;
    [[nodiscard]] virtual result<bool> verify_bls48581(const bls48581_public_key& key, byte_view message, const bls48581_signature& signature) = 0;
    [[nodiscard]] virtual result<bytes> create_range_proof(byte_view confidential_values) = 0;
    [[nodiscard]] virtual result<bool> verify_range_proof(byte_view proof) = 0;
    [[nodiscard]] virtual result<bytes> create_traversal_proof(byte_view request) = 0;
};

using protocol_provider_ptr = std::shared_ptr<protocol_provider>;

} // namespace quilibrium::crypto
