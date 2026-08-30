module;
#include <coroutine>
#include <expected>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

module quilibrium.protocol;
import quilibrium.net;

namespace quilibrium::protocol {
std::vector<std::string_view> method_registry::methods(service value) {
    switch (value) {
        case service::node: return {"GetPeerInfo","GetNodeInfo","GetWorkerInfo","Send","GetTokensByAccount","GetMetrics","GetVertexData","GetHyperedgeData","CreateTraversalProof","GetShardInfo","RequestJoin","SetManuallyManaged","GetLatestFrame","SubmitMessage"};
        case service::connectivity: return {"TestConnectivity"};
        case service::global: return {"GetGlobalFrame","GetGlobalProposal","GetAppShards","GetGlobalShards","GetLockedAddresses","GetWorkerInfo","StreamGlobalMessages","SubmitGlobalMessage"};
        case service::app_shard: return {"GetAppShardFrame","GetAppShardProposal"};
        case service::hypergraph_comparison: return {"HyperStream","GetChildrenForPath","PerformSync"};
        case service::key_registry: return {"GetKeyRegistry","GetKeyRegistryByProver","PutIdentityKey","PutProvingKey","PutCrossSignature","PutSignedKey","GetIdentityKey","GetProvingKey","GetSignedKey","GetSignedKeysByParent","RangeProvingKeys","RangeIdentityKeys","RangeSignedKeys"};
        case service::dispatch: return {"PutInboxMessage","GetInboxMessages","PutHub","GetHub","Sync"};
        case service::mixnet: return {"PutMessage","RoundStream"};
        case service::onion: return {"Connect"};
        case service::pubsub_proxy: return {"PublishToBitmask","Publish","Subscribe","Unsubscribe","ValidatorStream","GetPeerID","GetPeerstoreCount","GetNetworkPeersCount","GetRandomPeer","GetMultiaddrOfPeer","GetMultiaddrOfPeerStream","GetOwnMultiaddrs","GetNetworkInfo","GetPeerScore","SetPeerScore","AddPeerScore","Reconnect","Bootstrap","DiscoverPeers","IsPeerConnected","GetNetwork","Reachability","SignMessage","GetPublicKey"};
        case service::data_ipc: return {"Respawn","CreateJoinProof","SetHalted"};
        case service::ferret_proxy: return {"AliceProxy","BobProxy"};
    }
    return {};
}

bool method_registry::is_operator_only(service value) noexcept {
    return value == service::data_ipc || value == service::ferret_proxy || value == service::pubsub_proxy;
}


struct client::impl final {
    net::endpoint_pool endpoints;
    transport_ptr rpc;
    impl(std::vector<endpoint> values, transport_ptr value_rpc) : endpoints(std::move(values)), rpc(std::move(value_rpc)) {}
};

client::client(std::vector<endpoint> endpoints, transport_ptr rpc) : impl_(std::make_unique<impl>(std::move(endpoints), std::move(rpc))) {}
client::~client() = default;
client::client(client&&) noexcept = default;
client& client::operator=(client&&) noexcept = default;

task<result<rpc_frame>> client::call(rpc_call value, call_options options) {
    if (!impl_->rpc) co_return std::unexpected(error{.domain=error_domain::configuration, .code=200, .message="RPC transport is not configured"});
    auto selected = impl_->endpoints.select();
    if (!selected) co_return std::unexpected(selected.error());
    co_return sync_wait(impl_->rpc->unary(*selected, std::move(value), options));
}

} // namespace quilibrium::protocol
