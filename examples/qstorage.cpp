#include <cstdlib>
#include <iostream>
#include <string>

import quilibrium;

int main() {
    const char* access_key = std::getenv("Q_ACCESS_KEY_ID");
    const char* secret_key = std::getenv("Q_SECRET_ACCESS_KEY");
    if (!access_key || !secret_key) {
        std::cerr << "Set Q_ACCESS_KEY_ID and Q_SECRET_ACCESS_KEY\n";
        return 2;
    }

    quilibrium::sdk_config config;
    config.qstorage_credentials = quilibrium::sdk_credentials{
        .access_key_id = access_key,
        .secret_access_key = secret_key
    };

    auto connected = quilibrium::connect(std::move(config));
    if (!connected) {
        std::cerr << connected.error().message << '\n';
        return 1;
    }

    std::string text = "hello from quilibrium-cpp-sdk";
    const auto view = quilibrium::as_bytes(text);
    quilibrium::bytes payload(view.begin(), view.end());

    auto response = quilibrium::sync_wait(
        connected->storage().put("sdk-example", "hello.txt", std::move(payload), "text/plain")
    );
    if (!response) {
        std::cerr << response.error().message << '\n';
        return 3;
    }

    std::cout << "HTTP " << response->status_code << '\n';
}
