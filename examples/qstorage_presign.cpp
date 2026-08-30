#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

import quilibrium;

int main(int argc,char** argv) {
    const char* access=std::getenv("Q_ACCESS_KEY_ID");
    const char* secret=std::getenv("Q_SECRET_ACCESS_KEY");
    if (access==nullptr || secret==nullptr || argc<3) {
        std::cerr << "Usage: Q_ACCESS_KEY_ID=... Q_SECRET_ACCESS_KEY=... "
                     "quilibrium_qstorage_presign_example <bucket> <key> [content-type]\n";
        return 1;
    }

    quilibrium::sdk_config config{};
    config.qstorage_credentials=quilibrium::sdk_credentials{.access_key_id=access,.secret_access_key=secret};
    if (const char* region=std::getenv("Q_STORAGE_REGION")) config.qstorage_region=region;
    if (const char* endpoint_url=std::getenv("Q_STORAGE_ENDPOINT")) {
        auto endpoint=quilibrium::parse_endpoint(endpoint_url);
        if (!endpoint) { std::cerr << endpoint.error().message << '\n'; return 1; }
        config.qstorage_endpoints={std::move(*endpoint)};
    }

    auto q=quilibrium::connect(std::move(config));
    if (!q) { std::cerr << q.error().message << '\n'; return 1; }

    const std::string bucket=argv[1];
    const std::string key=argv[2];
    const std::string content_type=argc>3?argv[3]:"application/octet-stream";

    auto upload=q->storage().presign_put(bucket,key,content_type,std::chrono::minutes{15});
    if (!upload) { std::cerr << upload.error().message << '\n'; return 1; }

    std::cout << "Presigned PUT URL:\n" << upload->url << "\n\nRequired headers:\n";
    for (const auto& [name,value]:upload->required_headers) {
        std::cout << name << ": " << value << '\n';
    }

    auto download=q->storage().presign_get(bucket,key,std::chrono::minutes{15});
    if (!download) { std::cerr << download.error().message << '\n'; return 1; }
    std::cout << "\nPresigned GET URL:\n" << download->url << '\n';
    return 0;
}
