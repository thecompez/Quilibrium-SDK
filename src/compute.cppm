module;
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

export module quilibrium.compute;
import quilibrium.core;

export namespace quilibrium::compute {

enum class context_kind : std::uint8_t { intrinsic, hypergraph, extrinsic };

struct configuration final {
    ed448_public_key read_public_key{};
    ed448_public_key write_public_key{};
    bls48581_public_key owner_public_key{};
};
struct deploy final { configuration config{}; bytes rdf_schema{}; };
struct update final { configuration config{}; bytes rdf_schema{}; bytes owner_bls_signature{}; };
struct code_deploy final { address32 domain{}; bytes qcl_circuit{}; bytes signature{}; };
struct code_execute final { address32 domain{}; bytes proof_of_payment{}; bytes rendezvous{}; bytes operations{}; };
struct code_finalize final { address32 domain{}; bytes result{}; bytes state_changes{}; bytes proof{}; bytes output{}; };

/** Forward-compatible intrinsic identifier for Token/Hypergraph/Compute and newer network intrinsics. */
struct intrinsic_id final { std::string name{}; std::uint32_t version{}; };

} // namespace quilibrium::compute
