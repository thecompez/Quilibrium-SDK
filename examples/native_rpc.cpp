#include <iostream>
#include <string>

import quilibrium;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: native_rpc_example https://node-rpc.example\n";
        return 2;
    }

    auto endpoint = quilibrium::parse_endpoint(argv[1]);
    if (!endpoint) {
        std::cerr << endpoint.error().message << '\n';
        return 2;
    }

    quilibrium::sdk_config config;
    config.protocol_endpoints.push_back(*endpoint);
    auto connected = quilibrium::connect(std::move(config));
    if (!connected) return 1;

    auto response = quilibrium::sync_wait(
        connected->native().call(quilibrium::native_service::node, "GetNodeInfo", {})
    );
    if (!response) {
        std::cerr << response.error().message << '\n';
        return 3;
    }

    std::cout << "GetNodeInfo protobuf response: " << response->size() << " bytes\n";
}
