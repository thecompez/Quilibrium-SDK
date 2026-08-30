#include <cstdlib>
#include <iostream>
#include <string>

import quilibrium;

int main(int argc, char** argv) {
    const char* access_key = std::getenv("Q_ACCESS_KEY_ID");
    const char* secret_key = std::getenv("Q_SECRET_ACCESS_KEY");
    if (!access_key || !secret_key || argc < 2) {
        std::cerr << "Usage: qkms_example <key-id>; set Q_ACCESS_KEY_ID and Q_SECRET_ACCESS_KEY\n";
        return 2;
    }

    quilibrium::sdk_config config;
    config.qkms_credentials = quilibrium::sdk_credentials{
        .access_key_id = access_key,
        .secret_access_key = secret_key
    };

    auto connected = quilibrium::connect(std::move(config));
    if (!connected) return 1;

    const std::string payload = std::string{"{\"KeyId\":\""} + argv[1] + "\"}";
    auto response = quilibrium::sync_wait(
        connected->kms().invoke("DescribeKey", payload)
    );
    if (!response) {
        std::cerr << response.error().message << '\n';
        return 3;
    }

    std::cout.write(reinterpret_cast<const char*>(response->body.data()), static_cast<std::streamsize>(response->body.size()));
    std::cout << '\n';
}
