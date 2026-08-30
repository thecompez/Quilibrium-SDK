module;
#include <string>
#include <utility>
#include <vector>

export module quilibrium.hypergraph;
import quilibrium.core;

export namespace quilibrium::hypergraph {

struct configuration final {
    ed448_public_key read_public_key{};
    ed448_public_key write_public_key{};
    bls48581_public_key owner_public_key{};
};

struct deploy final { configuration config{}; bytes rdf_schema{}; };
struct update final { configuration config{}; bytes rdf_schema{}; bytes owner_bls_signature{}; };
struct vertex_add final { address32 domain{}; address32 data_address{}; bytes vector_commitment_tree{}; bytes ed448_signature{}; };
struct vertex_remove final { address32 domain{}; address32 data_address{}; bytes ed448_signature{}; };
struct hyperedge_add final { address32 domain{}; bytes serialized_hyperedge{}; bytes ed448_signature{}; };
struct hyperedge_remove final { address32 domain{}; bytes serialized_hyperedge{}; bytes ed448_signature{}; };

/** Source-level synchronization capabilities of HypergraphComparisonService. */
struct sync_capabilities final {
    bool hyper_stream{true};
    bool children_for_path{true};
    bool perform_sync{true};
};

} // namespace quilibrium::hypergraph
