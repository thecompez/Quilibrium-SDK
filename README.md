# Quilibrium C++ SDK

A modern **C++23 SDK for the Quilibrium ecosystem**, providing a unified native interface for HyperSnap, QStorage, QKMS, Quilibrium protocol RPCs, multi-peer routing, and cross-language integrations.

<img width="1672" height="941" alt="56923f1e-fac4-430a-a4ab-e6738411109f" src="https://github.com/user-attachments/assets/a38f6ae1-1b0b-4041-be36-9ad92367d9e3" />

```cpp
#include <iostream>

import quilibrium;

int main()
{
    auto q = quilibrium::connect();

    if (!q) {
        std::cerr << q.error().message << '\n';
        return 1;
    }

    auto user = quilibrium::sync_wait(
        q->hypersnap()
          .users()
          .get_by_fid(3)
    );

    if (!user) {
        std::cerr << user.error().message << '\n';
        return 1;
    }

    std::cout
        << user->display_name
        << " (@"
        << user->username
        << ")\n";
}
```

---

## Overview

Quilibrium C++ SDK provides a single native interface for building applications on top of the Quilibrium ecosystem.

It is designed for:

* Native desktop applications
* Qt / QML clients
* Farcaster clients
* Mini App backends
* Node and protocol tooling
* Infrastructure services
* Wallets and cryptographic applications
* Cross-language integrations

The SDK intentionally hides low-level transport, signing, routing, and protocol details behind higher-level APIs while keeping lower-level access available when required.

---

## Features

### HyperSnap

Typed access to Farcaster data exposed through HyperSnap.

```cpp
auto feed = quilibrium::sync_wait(
    q->hypersnap()
      .feeds()
      .trending(20)
);
```

Supported areas include:

* Users
* Casts
* Conversations
* Feeds
* Search
* Channels
* Reactions
* Follows
* Notifications
* Raw HyperSnap API access

HyperSnap read operations do not require credentials.

---

### QStorage

Native S3-compatible access to Quilibrium QStorage.

```cpp
auto uploaded = quilibrium::sync_wait(
    q->storage().put(
        "my-bucket",
        "hello.txt",
        data,
        "text/plain"
    )
);
```

Supported functionality includes:

* Bucket operations
* Object upload/download/delete
* Object metadata
* Multipart upload
* S3 subresources
* SigV4 authentication
* Raw S3-compatible requests

QStorage endpoint:

```text
https://qstorage.quilibrium.com
```

---

### QKMS

Native access to Quilibrium's multi-party Key Management System.

```cpp
auto response = quilibrium::sync_wait(
    q->kms().describe_key(
        R"({"KeyId":"YOUR_KEY_ID"})"
    )
);
```

Supported operation families include:

* Create / describe / list keys
* Enable / disable keys
* Encrypt / decrypt
* Sign / verify
* Generate data keys
* Generate / verify MAC
* Shared-secret derivation
* Public key retrieval
* Import key material
* Key policies
* Aliases
* Grants
* Tags
* Rotation
* Replication
* Scheduled deletion

Generic KMS operations are also available:

```cpp
auto response = quilibrium::sync_wait(
    q->kms().invoke(
        "EnableKeyRotation",
        R"({"KeyId":"..."})"
    )
);
```

---

### Native Quilibrium Protocol

The SDK exposes the native Quilibrium RPC surface without leaking gRPC implementation details into application code.

```cpp
auto response = quilibrium::sync_wait(
    q->native().call(
        quilibrium::native_service::node,
        "GetNodeInfo",
        {}
    )
);
```

Protocol service registry includes:

```text
NodeService
ConnectivityService
GlobalService
AppShardService
HypergraphComparisonService
KeyRegistryService
DispatchService
MixnetService
OnionService
PubSubProxy
DataIPCService
FerretProxy
```

Native protocol responses are currently exposed as protobuf payloads where generated high-level codecs are not yet available.

---

## Architecture

```text
                         Application
                             │
                             ▼
                   ┌───────────────────┐
                   │  quilibrium::sdk  │
                   └─────────┬─────────┘
                             │
          ┌──────────────────┼──────────────────┐
          │                  │                  │
          ▼                  ▼                  ▼
     HyperSnap           QStorage             QKMS
          │                  │                  │
          └──────────────┬───┴──────────────┬───┘
                         │                  │
                         ▼                  ▼
                  HTTP Transport        SigV4
                         │
                         ▼
                  Multi-Peer Routing
                         │
                         ▼
              Quilibrium Infrastructure

                             │
                             ▼
                   Native Protocol RPC
                             │
                             ▼
                      gRPC Framing
                             │
                             ▼
                    Protobuf Payloads
```

The public API is designed so that transport and protocol implementation details do not leak into application code.

---

## Module Layout

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

The umbrella module is:

```cpp
import quilibrium;
```

Lower-level modules may also be imported independently.

---

## SDK Surface

```text
quilibrium::sdk
│
├── hypersnap()
│   ├── users()
│   ├── casts()
│   ├── feeds()
│   └── raw()
│
├── storage()
│   ├── put()
│   ├── get()
│   ├── remove()
│   ├── multipart()
│   └── execute()
│
├── kms()
│   ├── create_key()
│   ├── describe_key()
│   ├── encrypt()
│   ├── decrypt()
│   ├── sign()
│   ├── verify()
│   └── invoke()
│
└── native()
    └── call()
```

---

## Requirements

Minimum recommended toolchain:

```text
CMake 3.30+
C++23 compiler
OpenSSL 3
libcurl
Ninja (recommended)
```

Tested during development with:

```text
GCC 14
LLVM / Clang
macOS
Linux
```

C++ Modules support depends on the compiler and build system version.

---

## Build

```bash
cmake \
    -S . \
    -B build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release

cmake --build build
```

To build with strict compiler warnings:

```bash
cmake \
    -S . \
    -B build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DQUILIBRIUM_WARNINGS_AS_ERRORS=ON

cmake --build build
```

---

## Tests

```bash
ctest \
    --test-dir build \
    --output-on-failure
```

The test suite covers:

```text
Core
JSON
SDK facade
Service integrations
C ABI
Python binding
```

---

## Install

```bash
cmake --install build \
    --prefix "$HOME/.local/quilibrium"
```

Then consume the SDK from another CMake project:

```cmake
find_package(
    Quilibrium
    CONFIG
    REQUIRED
)

target_link_libraries(
    my_application
    PRIVATE
    Quilibrium::SDK
)
```

---

## HyperSnap Example

HyperSnap public reads require no credentials.

```cpp
#include <iostream>

import quilibrium;

int main()
{
    auto result = quilibrium::connect();

    if (!result) {
        std::cerr << result.error().message << '\n';
        return 1;
    }

    auto& q = *result;

    auto feed = quilibrium::sync_wait(
        q.hypersnap()
         .feeds()
         .trending(20)
    );

    if (!feed) {
        std::cerr << feed.error().message << '\n';
        return 1;
    }

    for (const auto& cast : feed->casts) {
        std::cout
            << '@'
            << cast.author.username
            << ": "
            << cast.text
            << "\n\n";
    }
}
```

---

## QStorage Example

Set your QConsole credentials:

```bash
export Q_ACCESS_KEY_ID="..."
export Q_SECRET_ACCESS_KEY="..."
```

Configure the SDK:

```cpp
quilibrium::sdk_config config;

config.qstorage_credentials = {
    .access_key_id = access_key,
    .secret_access_key = secret_key
};

auto q = quilibrium::connect(
    std::move(config)
).value();
```

Upload:

```cpp
auto result = quilibrium::sync_wait(
    q.storage().put(
        "my-bucket",
        "hello.txt",
        data,
        "text/plain"
    )
);
```

Download:

```cpp
auto result = quilibrium::sync_wait(
    q.storage().get(
        "my-bucket",
        "hello.txt"
    )
);
```

---

## QKMS Example

```cpp
quilibrium::sdk_config config;

config.qkms_credentials = {
    .access_key_id = access_key,
    .secret_access_key = secret_key
};

auto q = quilibrium::connect(
    std::move(config)
).value();

auto response = quilibrium::sync_wait(
    q.kms().invoke(
        "CreateKey",
        R"({
            "Description":"SDK test key",
            "KeyUsage":"ENCRYPT_DECRYPT",
            "CustomerMasterKeySpec":"SYMMETRIC_DEFAULT"
        })"
    )
);
```

---

## Native RPC Example

```cpp
quilibrium::sdk_config config;

config.protocol_endpoint =
    *quilibrium::parse_endpoint(
        "https://your-quilibrium-node.example"
    );

auto q = quilibrium::connect(
    std::move(config)
).value();

auto response = quilibrium::sync_wait(
    q.native().call(
        quilibrium::native_service::node,
        "GetNodeInfo",
        {}
    )
);
```

---

## Examples

```text
examples/
├── basic.cpp
├── hypersnap_client.cpp
├── qstorage.cpp
├── qkms.cpp
├── native_rpc.cpp
├── qt/
└── miniapp_backend/
```

### Qt / QML

The Qt example demonstrates the recommended architecture:

```text
QML
 │
 ▼
Qt ViewModel / Controller
 │
 ▼
Quilibrium C++ SDK
 │
 ▼
HyperSnap / Quilibrium
```

QML never needs to communicate directly with the Quilibrium protocol layer.

---

## Mini Apps

For browser-based applications, the recommended architecture is:

```text
Browser / Mini App
        │
        ▼
     HTTPS API
        │
        ▼
   C++ Backend
        │
        ▼
Quilibrium C++ SDK
        │
        ▼
    Quilibrium
```

Secrets such as QStorage and QKMS credentials should remain on the backend.

---

## Cross-Language Bindings

The C++ implementation acts as the canonical SDK core.

```text
                    C++23 Core
                        │
                        ▼
                 Stable C ABI
                        │
       ┌────────────────┼─────────────────┐
       │                │                 │
       ▼                ▼                 ▼
    Python             Rust             Node
       │
       ├────────────── .NET
       │
       └────────────── other FFI runtimes
```

Bindings are located under:

```text
bindings/
├── c/
├── python/
├── rust/
├── node/
└── dotnet/
```

This avoids reimplementing Quilibrium protocol logic independently in every language.

---

## Design Principles

The SDK follows several core principles:

### One high-level SDK

Application developers should normally need only:

```cpp
import quilibrium;
```

### Native C++

The core implementation is written in modern C++23.

### Protocol-aware

The SDK models Quilibrium concepts instead of acting only as a collection of HTTP wrappers.

### Transport isolation

HTTP, gRPC framing, routing, and retry behavior remain internal implementation details.

### Escape hatches

Advanced users can access lower-level service APIs when the high-level facade does not expose a specific operation.

### Multi-peer aware

Network-facing services are designed with endpoint selection, retry, health tracking, and failover in mind.

### Cross-language core

Other language SDKs should use the C++ implementation through a stable ABI rather than duplicating protocol logic.

---

## Current Service Coverage

| Area                            | Status                               |
| ------------------------------- | ------------------------------------ |
| HyperSnap reads                 | Supported                            |
| HyperSnap typed users           | Supported                            |
| HyperSnap typed casts           | Supported                            |
| HyperSnap typed feeds           | Supported                            |
| QStorage                        | Supported                            |
| QStorage SigV4                  | Supported                            |
| QStorage multipart              | Supported                            |
| QKMS                            | Supported                            |
| QKMS SigV4                      | Supported                            |
| Native unary RPC                | Supported                            |
| Protocol service registry       | Supported                            |
| C ABI                           | Supported                            |
| Python binding                  | Supported                            |
| Rust binding                    | Available                            |
| Node binding                    | Available                            |
| .NET binding                    | Available                            |
| Qt/QML integration example      | Available                            |
| Streaming native RPC            | Planned                              |
| Generated typed protobuf facade | Planned                              |
| Klearu mainnet integration      | Waiting for stable upstream contract |
| MetaVM mainnet integration      | Waiting for stable upstream contract |

---

## Repository Structure

```text
quilibrium-cpp-sdk/
│
├── bindings/
│   ├── c/
│   ├── dotnet/
│   ├── node/
│   ├── python/
│   └── rust/
│
├── cmake/
│
├── compat/
│   └── upstreams.json
│
├── docs/
│   ├── ARCHITECTURE.md
│   ├── LANGUAGE_BINDINGS.md
│   ├── SERVICE_MATRIX.md
│   └── UPSTREAM_RESEARCH.md
│
├── examples/
│   ├── qt/
│   ├── miniapp_backend/
│   ├── hypersnap_client.cpp
│   ├── native_rpc.cpp
│   ├── qkms.cpp
│   └── qstorage.cpp
│
├── src/
│
├── tests/
│
├── BUILD-VERIFICATION.md
├── LICENSE-NOTICE.md
├── RELEASE-NOTES.md
├── CMakeLists.txt
└── README.md
```

---

## Upstream Compatibility

Quilibrium evolves quickly.

The SDK keeps upstream compatibility information under:

```text
compat/upstreams.json
```

Protocol-sensitive implementations should be updated against pinned upstream revisions rather than relying on undocumented behavior.

---

## Security

Do not embed QStorage or QKMS secret credentials in browser applications.

For Mini Apps and web applications, keep privileged operations on a trusted backend.

Cryptographic and threshold-key behavior should be treated as security-sensitive code and reviewed before production deployment.

---

## Licensing

This project interfaces with upstream Quilibrium projects that may use different licenses.

See:

```text
LICENSE-NOTICE.md
```

before redistributing upstream-derived implementation code.

---

## Status

Quilibrium C++ SDK is under active development.

The current release establishes the native SDK architecture, high-level facade, HTTP transport, S3/KMS integrations, HyperSnap client surface, native protocol transport, cross-language ABI, examples, packaging, and test infrastructure.

Future releases can expand typed protocol codecs, streaming RPC support, cryptographic backends, and newly stabilized Quilibrium services.

---

## Example Applications

The SDK can serve as the infrastructure layer for:

```text
Farcaster native clients
Social applications
Wallets
Quilibrium node tools
QStorage applications
Secure key-management systems
Mini App backends
Desktop clients
Developer tooling
Infrastructure services
```

---

## License

See `LICENSE-NOTICE.md` for licensing and upstream attribution information.
