module;
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

export module quilibrium.token;
import quilibrium.core;

export namespace quilibrium::token {

enum behavior : std::uint32_t {
    none = 0,
    mintable = 1,
    burnable = 2,
    divisible = 4,
    acceptable = 8,
    expirable = 16,
    tenderable = 32
};

enum class mint_behavior : std::uint8_t { none, proof, authority, signature, payment };
enum class proof_basis : std::uint8_t { none, proof_of_meaningful_work, verkle_multiproof_with_signature };

struct authority final { std::uint32_t key_type{}; bytes public_key{}; bool can_burn{}; };
struct fee_basis final { bytes baseline{}; };
struct mint_strategy final {
    mint_behavior mode{mint_behavior::none};
    proof_basis proof{proof_basis::none};
    bytes verkle_root{};
    std::optional<authority> mint_authority{};
    bytes payment_address{};
    std::optional<fee_basis> fee{};
};

/** Mirrors TokenConfiguration without exposing generated protobuf types. */
struct configuration final {
    std::uint32_t flags{behavior::none};
    std::optional<mint_strategy> strategy{};
    bytes units{};
    bytes supply{};
    std::string name{};
    std::string symbol{};
    std::vector<address64> additional_references{};
    bls48581_public_key owner_public_key{};
};

struct deploy final { configuration config{}; bytes rdf_schema{}; };
struct update final { configuration config{}; bytes rdf_schema{}; bytes owner_bls_signature{}; };

struct transaction_input final { bytes commitment{}; bytes signature{}; std::vector<bytes> proofs{}; };
struct recipient_bundle final { bytes one_time_key{}; bytes verification_key{}; bytes coin_balance{}; bytes mask{}; bytes additional_reference{}; bytes additional_reference_key{}; };
struct transaction_output final { bytes frame_number{}; bytes commitment{}; recipient_bundle recipient{}; };
struct transaction final { address32 domain{}; std::vector<transaction_input> inputs{}; std::vector<transaction_output> outputs{}; std::vector<bytes> fees{}; bytes range_proof{}; bytes traversal_proof{}; };

} // namespace quilibrium::token
