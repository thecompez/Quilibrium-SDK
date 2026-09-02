# Quilibrium C++ SDK

A modern **C++23 SDK for the Quilibrium ecosystem** with native C++ modules, coroutine-aware APIs, explicit error handling, multi-peer routing, and cross-language bindings.

<img width="1672" height="941" alt="Quilibrium C++ SDK" src="https://github.com/user-attachments/assets/a38f6ae1-1b0b-4041-be36-9ad92367d9e3" />

The SDK provides a unified interface for:

* **HyperSnap** — Farcaster users, casts, feeds, conversations, search, reactions, follows, notifications, and raw access.
* **QStorage** — authenticated S3-compatible storage, multipart transfers, and SigV4 presigned URLs.
* **QKMS** — key management, encryption, signing, verification, policies, aliases, grants, rotation, and related operations.
* **Quilibrium protocol RPC** — native unary gRPC framing and protocol service dispatch.
* **Multi-peer routing** — endpoint selection, retries, and failover.
* **Cross-language integration** — stable C ABI with Python, Rust, Node.js, and .NET wrappers.
* **C++23 modules** — applications normally need only `import quilibrium;`.

Version: **1.1.0**

## Modern C++ Quick Start

```cpp
#include <print>

import quilibrium;

int main()
{
    const auto sdk = quilibrium::connect();

    if (!sdk) {
        std::println("Unable to initialize Quilibrium SDK: {}",
                     sdk.error().message);
        return 1;
    }

    const auto user = quilibrium::sync_wait(
        sdk->hypersnap()
            .users()
            .get_by_fid(3)
    );

    if (!user) {
        std::println("HyperSnap request failed: {}",
                     user.error().message);
        return 1;
    }

    std::println(
        "{} (@{}) — FID {}",
        user->display_name,
        user->username,
        user->fid
    );
}
```

The SDK uses explicit result types rather than exceptions for normal recoverable service failures:

```cpp
quilibrium::result<T>
```

which is backed by C++23:

```cpp
std::expected<T, quilibrium::error>
```

This makes success and failure part of the type contract.

---

## Coroutine-native usage

SDK operations return lightweight `task<result<T>>` values and can be consumed directly through C++ coroutines.

```cpp
#include <print>

import quilibrium;

quilibrium::task<int> run()
{
    auto sdk = quilibrium::connect();

    if (!sdk) {
        std::println("Connection error: {}", sdk.error().message);
        co_return 1;
    }

    const auto user = co_await sdk->hypersnap()
        .users()
        .get_by_fid(3);

    if (!user) {
        std::println("HyperSnap error: {}", user.error().message);
        co_return 1;
    }

    std::println(
        "Resolved @{} with FID {}",
        user->username,
        user->fid
    );

    co_return 0;
}

int main()
{
    return quilibrium::sync_wait(run());
}
```

`sync_wait()` remains available for command-line programs, worker threads, tests, and environments where a synchronous composition boundary is more appropriate.

---

# HyperSnap

HyperSnap provides typed access to Farcaster data while retaining a raw HTTP escape hatch.

## Get a user

```cpp
const auto user = quilibrium::sync_wait(
    sdk->hypersnap()
        .users()
        .get_by_username("compez.eth")
);

if (!user) {
    std::println("Request failed: {}", user.error().message);
    return 1;
}

std::println(
    "{} — {} followers",
    user->display_name,
    user->follower_count
);
```

## Trending feed

```cpp
const auto feed = quilibrium::sync_wait(
    sdk->hypersnap()
        .feeds()
        .trending(20)
);

if (!feed) {
    std::println("Unable to load feed: {}", feed.error().message);
    return 1;
}

for (const auto& cast : feed->casts) {
    std::println(
        "@{}: {}",
        cast.author.username,
        cast.text
    );
}
```

## Search users

```cpp
const auto users = quilibrium::sync_wait(
    sdk->hypersnap()
        .users()
        .search("quilibrium", 10)
);

if (!users) {
    std::println("Search failed: {}", users.error().message);
    return 1;
}

for (const auto& user : *users) {
    std::println(
        "{} (@{})",
        user.display_name,
        user.username
    );
}
```

## Raw HyperSnap access

Typed APIs cover common operations, while new or specialized HyperSnap endpoints can be accessed without waiting for a new SDK release.

```cpp
const auto response = quilibrium::sync_wait(
    sdk->hypersnap().get(
        "/v2/farcaster/user",
        {
            {"fid", "3"}
        }
    )
);

if (!response) {
    std::println("HyperSnap request failed: {}",
                 response.error().message);
    return 1;
}

std::println("HTTP {}", response->status_code);
```

---

# QStorage

QStorage provides authenticated S3-compatible operations as well as temporary SigV4 capabilities for untrusted clients.

## Configure credentials

```cpp
quilibrium::sdk_config config {};

config.qstorage_credentials = quilibrium::sdk_credentials {
    .access_key_id = "YOUR_ACCESS_KEY",
    .secret_access_key = "YOUR_SECRET_KEY"
};

config.qstorage_region = "q";

auto sdk = quilibrium::connect(std::move(config));

if (!sdk) {
    std::println("SDK initialization failed: {}",
                 sdk.error().message);
    return 1;
}
```

Secrets should only be configured inside trusted applications or services.

Do not embed QStorage credentials inside browser bundles, distributed desktop applications, mobile applications, or Mini Apps.

## Upload an object

```cpp
quilibrium::bytes data {
    std::byte {'H'},
    std::byte {'e'},
    std::byte {'l'},
    std::byte {'l'},
    std::byte {'o'}
};

const auto uploaded = quilibrium::sync_wait(
    sdk->storage().put(
        "my-bucket",
        "hello.txt",
        std::move(data),
        "text/plain"
    )
);

if (!uploaded) {
    std::println("Upload failed: {}", uploaded.error().message);
    return 1;
}

std::println("Upload completed with HTTP {}",
             uploaded->status_code);
```

## Download an object

```cpp
const auto downloaded = quilibrium::sync_wait(
    sdk->storage().get(
        "my-bucket",
        "hello.txt"
    )
);

if (!downloaded) {
    std::println("Download failed: {}",
                 downloaded.error().message);
    return 1;
}

std::println(
    "Received {} bytes",
    downloaded->body.size()
);
```

---

# QStorage Presigned URLs

Presigned URLs allow an untrusted client to perform one specific QStorage operation without receiving the secret key.

```text
Client
   │
   │ Request temporary authorization
   ▼
Trusted application / backend
   │
   │ Quilibrium SDK
   ▼
presign_put(...)
   │
   ▼
Temporary URL + required signed headers
   │
   ▼
Client ─────────────────────────────► QStorage
             Direct upload
```

The payload does not need to pass through your backend.

## Presigned PUT

```cpp
#include <chrono>
#include <print>

import quilibrium;

int main()
{
    quilibrium::sdk_config config {};

    config.qstorage_credentials = {
        .access_key_id = "YOUR_ACCESS_KEY",
        .secret_access_key = "YOUR_SECRET_KEY"
    };

    config.qstorage_region = "q";

    auto sdk = quilibrium::connect(std::move(config));

    if (!sdk) {
        std::println(
            "SDK initialization failed: {}",
            sdk.error().message
        );

        return 1;
    }

    const auto upload = sdk->storage().presign_put(
        "my-bucket",
        "uploads/image.jpg",
        "image/jpeg",
        std::chrono::minutes {15}
    );

    if (!upload) {
        std::println(
            "Unable to create presigned URL: {}",
            upload.error().message
        );

        return 1;
    }

    std::println("PUT {}", upload->url);

    for (const auto& [name, value] : upload->required_headers) {
        std::println("{}: {}", name, value);
    }
}
```

When `Content-Type` is supplied, it becomes part of the signature contract.

The client must send exactly the required value:

```text
content-type: image/jpeg
```

Changing or omitting a signed header invalidates the request.

## Presigned GET

```cpp
const auto download = sdk->storage().presign_get(
    "my-bucket",
    "uploads/image.jpg",
    std::chrono::minutes {15}
);

if (!download) {
    std::println(
        "Unable to create download URL: {}",
        download.error().message
    );

    return 1;
}

std::println("GET {}", download->url);
```

## Presigned HEAD

```cpp
const auto metadata = sdk->storage().presign_head(
    "my-bucket",
    "uploads/image.jpg",
    std::chrono::minutes {5}
);
```

SigV4 presigned URL expiration may range from **1 second to 7 days**.

---

# QKMS

QKMS operations use the same explicit `task<result<T>>` model.

```cpp
const auto response = quilibrium::sync_wait(
    sdk->kms().describe_key(
        R"({"KeyId":"YOUR_KEY_ID"})"
    )
);

if (!response) {
    std::println(
        "QKMS request failed: {}",
        response.error().message
    );

    return 1;
}

std::println(
    "QKMS returned HTTP {}",
    response->status_code
);
```

The high-level API covers operations including:

* key creation and description
* encryption and decryption
* signing and verification
* data-key generation
* MAC operations
* shared-secret derivation
* public-key retrieval
* imported key material
* key policies
* aliases
* grants
* tags
* rotation
* replication
* scheduled deletion

The generic `invoke()` API remains available for compatible QKMS operations that do not yet have a dedicated facade function.

---

# Native Quilibrium Protocol

The SDK can issue raw-protobuf unary calls against registered Quilibrium protocol services.

```cpp
const auto response = quilibrium::sync_wait(
    sdk->native().call(
        quilibrium::native_service::node,
        "GetNodeInfo",
        {}
    )
);

if (!response) {
    std::println(
        "Protocol call failed: {}",
        response.error().message
    );

    return 1;
}

std::println(
    "Received {} protobuf bytes",
    response->size()
);
```

Registered services include:

* NodeService
* ConnectivityService
* GlobalService
* AppShardService
* HypergraphComparisonService
* KeyRegistryService
* DispatchService
* MixnetService
* OnionService
* PubSubProxy
* DataIPCService
* FerretProxy

---

# Multi-peer Routing

Applications can configure multiple service endpoints instead of depending on a single peer.

```cpp
quilibrium::sdk_config config {};

config.hypersnap_endpoints = {
    {
        .scheme = "https",
        .host = "haatz.quilibrium.com",
        .port = 443
    },
    {
        .scheme = "https",
        .host = "another-peer.example",
        .port = 443
    }
};

const auto sdk = quilibrium::connect(
    std::move(config)
);
```

The routing layer provides primitives for endpoint selection, retries, and failover.

Per-call behavior can also be controlled explicitly:

```cpp
quilibrium::call_options options {
    .timeout = std::chrono::seconds {10},
    .max_attempts = 3,
    .allow_failover = true,
    .idempotent = true
};

const auto feed = quilibrium::sync_wait(
    sdk->hypersnap()
        .feeds()
        .trending(
            20,
            {},
            options
        )
);
```

---

# Architecture

```text
                           Application
                               │
                               ▼
                     ┌───────────────────┐
                     │  quilibrium::sdk  │
                     └─────────┬─────────┘
                               │
         ┌─────────────────────┼─────────────────────┐
         │                     │                     │
         ▼                     ▼                     ▼
    HyperSnap              QStorage                QKMS
         │                     │                     │
         │             ┌───────┴────────┐            │
         │             │                │            │
         │             ▼                ▼            │
         │      Authenticated       Presigning       │
         │          SigV4              SigV4         │
         │                              │            │
         │                              ▼            │
         │                   Browser / Desktop       │
         │                    Mobile / Worker        │
         │                              │            │
         │                              ▼            │
         │                          QStorage          │
         │                                           │
         └──────────────────┬────────────────────────┘
                            │
                            ▼
                  Native Protocol RPC
                            │
                            ▼
                       gRPC framing
                            │
                            ▼
                    Protobuf payloads
```

---

# C++ Module Layout

The SDK is implemented using project-owned C++ modules.

```text
src/
├── core.cppm
├── json.cppm
├── net.cppm
├── http.cppm
├── curl_transport.cppm
├── sigv4.cppm
├── crypto.cppm
├── protocol.cppm
├── token.cppm
├── hypergraph.cppm
├── compute.cppm
├── qstorage.cppm
├── qkms.cppm
├── hypersnap.cppm
├── farcaster.cppm
├── sdk_facade.cppm
└── sdk.cppm
```

Most applications only need:

```cpp
import quilibrium;
```

Project-owned APIs are exposed through modules rather than a traditional public-header architecture.

The C header exists specifically as an ABI boundary for non-C++ runtimes.

---

# Build

Requirements:

```text
CMake 3.30+
C++23 compiler
OpenSSL 3
libcurl
Ninja recommended
```

Configure and build:

```bash
cmake \
    -S . \
    -B build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release

cmake --build build --parallel
```

Strict warnings:

```bash
cmake \
    -S . \
    -B build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DQUILIBRIUM_WARNINGS_AS_ERRORS=ON

cmake --build build --parallel
```

Run tests:

```bash
ctest \
    --test-dir build \
    --output-on-failure \
    --no-tests=error
```

---

# Install and Consume from C++

Install:

```bash
cmake --install build \
    --prefix "$HOME/.local/quilibrium"
```

Consumer CMake:

```cmake
find_package(Quilibrium CONFIG REQUIRED)

target_link_libraries(
    my_application
    PRIVATE
        Quilibrium::SDK
)
```

Consumer source:

```cpp
import quilibrium;
```

Consumers do not need to depend on internal module filenames.

---

# Language Bindings

C++ is the canonical implementation.

Other runtimes use the stable C ABI exported by `libquilibrium`.

```text
                         C++23 Core
                            │
                            ▼
                     libquilibrium
                            │
                            ▼
                        Stable C ABI
                            │
          ┌─────────┬───────┼────────┬─────────┐
          │         │       │        │         │
          ▼         ▼       ▼        ▼         ▼
          C       Python   Rust    Node.js    .NET
```

Available bindings:

```text
bindings/
├── c/
├── python/
├── rust/
├── node/
└── dotnet/
```

---

## Python

```python
from quilibrium_sdk import SDK

with SDK(
    hypersnap_endpoint="https://haatz.quilibrium.com"
) as sdk:
    user = sdk.user_by_fid(3)

    print(user.json())
```

Configure the native library when it is not installed in the system library path:

```bash
export QUILIBRIUM_SDK_LIB=/path/to/libquilibrium.so
export PYTHONPATH=/path/to/Quilibrium-SDK/bindings/python
```

On macOS:

```bash
export QUILIBRIUM_SDK_LIB=/path/to/libquilibrium.dylib
```

Python supports the HyperSnap, QStorage, QKMS, and native protocol bridge exposed by the stable ABI.

---

## Rust

```rust
use quilibrium_sdk::{
    Config,
    Sdk,
};

fn main() -> Result<(), quilibrium_sdk::Error>
{
    let sdk = Sdk::new(Config {
        hypersnap_endpoint: Some(
            "https://haatz.quilibrium.com".into()
        ),
        ..Default::default()
    })?;

    let user = sdk.user_by_fid(3)?;

    println!(
        "{}",
        String::from_utf8_lossy(&user.body)
    );

    Ok(())
}
```

The Rust binding is a source FFI crate located under:

```text
bindings/rust
```

Set `QUILIBRIUM_SDK_LIB_DIR` as documented by the Rust binding when linking against a non-system installation.

---

## Node.js

The Node.js binding uses `koffi`.

```javascript
const {
    SDK
} = require("./bindings/node");

const sdk = new SDK({
    hypersnapEndpoint: "https://haatz.quilibrium.com"
});

try {
    const user = sdk.userByFid(3);

    console.log(
        user.body.toString("utf8")
    );
} finally {
    sdk.close();
}
```

The native library may be selected through:

```bash
export QUILIBRIUM_SDK_LIB=/path/to/libquilibrium.so
```

or on macOS:

```bash
export QUILIBRIUM_SDK_LIB=/path/to/libquilibrium.dylib
```

---

## .NET

```csharp
using Quilibrium;

using var sdk = new Sdk(
    hypersnapEndpoint: "https://haatz.quilibrium.com"
);

var user = sdk.UserByFid(3);

Console.WriteLine(
    user.Utf8Text
);
```

The .NET binding uses P/Invoke against the same native `quilibrium` library.

---

## C

The C ABI is the common portability boundary used by the language wrappers.

```c
#include <stdio.h>

#include "quilibrium.h"

int main(void)
{
    ql_sdk_config config = {
        .hypersnap_endpoint = "https://haatz.quilibrium.com"
    };

    ql_error error = {0};

    ql_sdk* sdk = ql_sdk_create(
        &config,
        &error
    );

    if (sdk == NULL) {
        fprintf(
            stderr,
            "Unable to create SDK: %s\n",
            error.message != NULL
                ? error.message
                : "unknown error"
        );

        ql_error_free(&error);
        return 1;
    }

    ql_response response = {0};

    const int result = ql_hypersnap_user_by_fid(
        sdk,
        3,
        &response,
        &error
    );

    if (result != 0) {
        fprintf(
            stderr,
            "HyperSnap request failed: %s\n",
            error.message != NULL
                ? error.message
                : "unknown error"
        );

        ql_error_free(&error);
        ql_sdk_destroy(sdk);
        return 1;
    }

    fwrite(
        response.body.data,
        1,
        response.body.size,
        stdout
    );

    fputc('\n', stdout);

    ql_response_free(&response);
    ql_error_free(&error);
    ql_sdk_destroy(sdk);

    return 0;
}
```

The C ABI intentionally remains a conventional header boundary because it defines the stable binary interface consumed by non-C++ languages.

---

# Binding Coverage

| Runtime | Integration    | HyperSnap | QStorage | QKMS | Native RPC |
| ------- | -------------- | --------: | -------: | ---: | ---------: |
| C++23   | Native modules |         ✅ |        ✅ |    ✅ |          ✅ |
| C       | Stable ABI     |         ✅ |        ✅ |    ✅ |          ✅ |
| Python  | C ABI wrapper  |         ✅ |        ✅ |    ✅ |          ✅ |
| Rust    | FFI wrapper    |         ✅ |        ✅ |    ✅ |          ✅ |
| Node.js | Koffi          |         ✅ |        ✅ |    ✅ |          ✅ |
| .NET    | P/Invoke       |         ✅ |        ✅ |    ✅ |          ✅ |

The C++ implementation remains the protocol and service core. Language bindings deliberately remain thin so protocol behavior is implemented once rather than independently reimplemented in each runtime.

---

# Examples

```text
examples/
├── basic.cpp
├── hypersnap_client.cpp
├── qstorage.cpp
├── qstorage_presign.cpp
├── qkms.cpp
├── native_rpc.cpp
├── qt/
└── miniapp_backend/
```

Examples should follow the same C++23 conventions as production-facing documentation:

* C++ modules for project APIs
* `std::print` / `std::println` for formatted console output
* explicit `std::expected`-style failure handling
* initialized variables
* `const` where mutation is unnecessary
* no broad `using namespace`
* readable formatting rather than compressed expressions
* scoped ownership and RAII
* explicit timeout and routing configuration when relevant

---

# Security

* Never commit QStorage or QKMS credentials.
* Never log secret keys.
* Never place long-lived secrets in browser, desktop, mobile, or Mini App distributions.
* Generate presigned capabilities inside trusted environments.
* Use least-privilege credentials.
* Keep presigned expirations as short as the workflow permits.
* Treat `required_headers` as part of the cryptographic signature contract.
* Treat a presigned URL as a temporary bearer capability.
* Secret access keys and derived SigV4 signing keys must never appear in generated URLs or error messages.

---

# Service Coverage

See:

```text
docs/SERVICE_MATRIX.md
```

for the detailed service matrix and intentionally reserved surfaces.

# Upstream Compatibility

Quilibrium evolves quickly.

Protocol-sensitive upstream compatibility information is pinned in:

```text
compat/upstreams.json
```

Applications relying on low-level protocol behavior should review this information when upgrading SDK versions.

# Licensing

This project interfaces with upstream Quilibrium projects that may use different licenses.

See:

```text
LICENSE-NOTICE.md
```

before redistributing upstream-derived implementation code.