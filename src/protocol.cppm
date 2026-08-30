module;
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

export module quilibrium.protocol;
import quilibrium.core;

export namespace quilibrium::protocol {
enum class service : std::uint8_t { node, connectivity, global, app_shard, hypergraph_comparison, key_registry, dispatch, mixnet, onion, pubsub_proxy, data_ipc, ferret_proxy };
enum class streaming : std::uint8_t { unary, server, client, bidirectional };
struct rpc_call final { service target_service{service::node}; std::string method{}; streaming mode{streaming::unary}; bytes protobuf_payload{}; };
struct rpc_frame final { bytes protobuf_payload{}; bool end_of_stream{true}; };
class transport {
public:
    virtual ~transport() = default;
    [[nodiscard]] virtual task<result<rpc_frame>> unary(endpoint target, rpc_call call, call_options options = {}) = 0;
};
using transport_ptr = std::shared_ptr<transport>;
class method_registry final {
public:
    [[nodiscard]] static std::vector<std::string_view> methods(service value);
    [[nodiscard]] static bool is_operator_only(service value) noexcept;
};
class client final {
public:
    client(std::vector<endpoint> endpoints, transport_ptr rpc);
    ~client();
    client(client&&) noexcept;
    client& operator=(client&&) noexcept;
    client(const client&) = delete;
    client& operator=(const client&) = delete;
    [[nodiscard]] task<result<rpc_frame>> call(rpc_call value, call_options options = {});
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};
} // namespace quilibrium::protocol
