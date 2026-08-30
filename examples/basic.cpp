#include <iostream>

import quilibrium;

int main() {
    using namespace quilibrium;
    net::endpoint_pool peers({
        {.scheme="https", .host="haatz.quilibrium.com", .port=443},
        {.scheme="https", .host="example-peer.invalid", .port=443}
    });
    auto selected = peers.select();
    if (!selected) {
        std::cerr << selected.error().message << '\n';
        return 1;
    }
    std::cout << selected->origin() << '\n';
    return 0;
}
