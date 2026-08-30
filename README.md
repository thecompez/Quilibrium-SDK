# Quilibrium C++ SDK

A modern **C++23 SDK for the Quilibrium ecosystem**, providing a unified native interface for HyperSnap, QStorage, QKMS, Quilibrium protocol RPCs, multi-peer routing, and cross-language integrations.

<img width="1672" height="941" alt="Quilibrium C++ SDK" src="https://github.com/user-attachments/assets/a38f6ae1-1b0b-4041-be36-9ad92367d9e3" />

Version: **1.1.0**

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
        q->hypersnap().users().get_by_fid(3)
    );

    if (!user) {
        std::cerr << user.error().message << '\n';
        return 1;
    }

    std::cout << user->display_name << " (@" << user->username << ")\n";
}
```

## Overview

Quilibrium C++ SDK provides a single native interface for building applications on top of the Quilibrium ecosystem.

It is designed for native desktop applications, Qt/QML clients, Farcaster clients, Mini App backends, infrastructure services, wallets, developer tooling, and other C++ applications that need direct access to Quilibrium services.

The SDK keeps low-level transport, signing, routing, and protocol details behind higher-level APIs while retaining escape hatches for advanced use cases.

## Features

- **HyperSnap** — typed users, casts, conversations, feeds and raw API access.
- **QStorage** — S3-compatible authenticated operations, multipart upload, and SigV4 presigned URLs.
- **QKMS** — KMS-compatible key, crypto, policy, grant, alias, rotation and related operations.
- **Native protocol RPC** — unary gRPC framing and service registry for Quilibrium protocol services.
- **Multi-peer routing** — endpoint selection, retry and failover primitives.
- **Cross-language ABI** — C bridge plus Python, Rust, Node.js and .NET bindings.
- **C++23 modules** — public API through named modules, including the umbrella `quilibrium` module.

---

# QStorage presigned URLs

Version 1.1 adds production-grade **AWS Signature Version 4 query presigning** with first-class QStorage integration.

A presigned URL grants temporary access to one specific request without exposing the QStorage secret key to the caller. This is particularly useful for direct uploads and downloads from browsers, desktop clients, mobile apps, workers, or other untrusted/distributed clients.

```text
Client
   │ request temporary upload authorization
   ▼
Trusted backend using Quilibrium SDK
   │
   ▼
presign_put(...)
   │
   ▼
Temporary SigV4 URL + required headers
   │
   ▼
Client ───────────────────────────────► QStorage
             direct object upload
```

The object bytes do **not** need to pass through the trusted backend.

> **Security:** QStorage access keys and secret keys must never be embedded in distributed desktop, mobile, or browser applications. Generate presigned URLs only in a trusted service or another protected execution environment.

## Presigned PUT

```cpp
#include <chrono>
#include <iostream>

import quilibrium;

int main()
{
    quilibrium::sdk_config config{};
    config.qstorage_credentials = {
        .access_key_id = "YOUR_ACCESS_KEY",
        .secret_access_key = "YOUR_SECRET_KEY"
    };

    // Explicitly configurable. Keep this aligned with your QStorage account/service configuration.
    config.qstorage_region = "q";

    auto q = quilibrium::connect(std::move(config));
    if (!q) {
        std::cerr << q.error().message << '\n';
        return 1;
    }

    auto upload = q->storage().presign_put(
        "my-bucket",
        "uploads/image.jpg",
        "image/jpeg",
        std::chrono::minutes{15}
    );

    if (!upload) {
        std::cerr << upload.error().message << '\n';
        return 1;
    }

    std::cout << upload->url << '\n';

    for (const auto& [name, value] : upload->required_headers) {
        std::cout << name << ": " << value << '\n';
    }
}
```

When a content type is supplied to `presign_put()`, it is cryptographically bound to the signature and is returned in `required_headers`.
The client must send the exact header value when using the URL.
For example:

```text
content-type: image/jpeg
```

Changing or omitting a signed header causes the remote service to reject the request with a signature mismatch.

## Presigned GET

```cpp
auto download = q->storage().presign_get(
    "my-bucket",
    "uploads/image.jpg",
    std::chrono::minutes{15}
);

if (download) {
    std::cout << download->url << '\n';
}
```

## Presigned HEAD

```cpp
auto metadata = q->storage().presign_head(
    "my-bucket",
    "uploads/image.jpg",
    std::chrono::minutes{5}
);
```

## Expiration

SigV4 presigned URLs accept expirations from **1 second through 7 days (604800 seconds)**.
Zero, negative, and longer durations are rejected instead of being silently clamped.

```cpp
auto result = q->storage().presign_get(
    "my-bucket",
    "object.bin",
    std::chrono::hours{1}
);
```

## Session credentials

Generic SigV4 presigning supports temporary credentials.
When a `session_token` is configured, the signer adds `X-Amz-Security-Token` to the canonical query string and final URL.
The secret access key is never included in the URL.

## Generic SigV4 presigning

Presigning itself is generic and is not tied to QStorage:

```cpp
import quilibrium.core;
import quilibrium.sigv4;

quilibrium::auth::sigv4_signer signer{
    {
        .access_key_id = "AKID",
        .secret_access_key = "SECRET"
    },
    "us-east-1",
    "s3"
};

quilibrium::http_request request{
    .verb = quilibrium::http_method::get,
    .target_endpoint = {
        .scheme = "https",
        .host = "storage.example.com",
        .port = 443
    },
    .target = "/bucket/object.txt?versionId=123"
};

auto presigned = signer.presign(
    std::move(request),
    {.expires = std::chrono::minutes{10}}
);
```

`auth::presigned_request` contains:

```cpp
std::string url;
quilibrium::http_headers required_headers;
std::chrono::system_clock::time_point expires_at;
```

The generic implementation preserves arbitrary existing query parameters, including duplicate keys and empty S3 subresources. This makes it suitable for later multipart-presigning APIs such as `?uploads` and `?partNumber=1&uploadId=...` without redesigning the signer.

QStorage also exposes a low-level extension point:

```cpp
quilibrium::qstorage::client storage{configuration};

auto part = storage.presign(
    quilibrium::http_method::put,
    "/bucket/large.bin?partNumber=1&uploadId=...",
    {},
    {.expires = std::chrono::minutes{10}}
);
```

## QStorage signing region

The QStorage signing region is configurable through both `qstorage::config::region` and `sdk_config::qstorage_region`.
The historical SDK default (`q`) is retained for source compatibility, but applications should configure the value expected by their current QStorage deployment/account rather than assuming a region string universally.

---

## HyperSnap

Typed access to Farcaster data exposed through HyperSnap:

```cpp
auto feed = quilibrium::sync_wait(
    q->hypersnap().feeds().trending(20)
);
```

Supported areas include users, casts, conversations, feeds, search, channels, reactions, follows, notifications, and raw HyperSnap reads.

---

## QStorage authenticated operations

Existing authenticated APIs remain unchanged:

```cpp
auto uploaded = quilibrium::sync_wait(
    q->storage().put(
        "my-bucket",
        "hello.txt",
        data,
        "text/plain"
    )
);

auto downloaded = quilibrium::sync_wait(
    q->storage().get("my-bucket", "hello.txt")
);

auto removed = quilibrium::sync_wait(
    q->storage().remove("my-bucket", "hello.txt")
);
```

The low-level QStorage client also supports bucket/object operations, copy/head/list, multipart create/upload/complete/abort/list operations, and a raw signed `execute()` escape hatch for S3-compatible subresources.

---

## QKMS

```cpp
auto response = quilibrium::sync_wait(
    q->kms().describe_key(
        R"({"KeyId":"YOUR_KEY_ID"})"
    )
);
```

The QKMS surface includes key creation/description/listing, enable/disable, encrypt/decrypt, sign/verify, data keys, MAC operations, shared-secret derivation, public key retrieval, imported key material, policies, aliases, grants, tags, rotation, replication and scheduled deletion.

---

## Native Quilibrium protocol

```cpp
auto response = quilibrium::sync_wait(
    q->native().call(
        quilibrium::native_service::node,
        "GetNodeInfo",
        {}
    )
);
```

The protocol registry includes NodeService, ConnectivityService, GlobalService, AppShardService, HypergraphComparisonService, KeyRegistryService, DispatchService, MixnetService, OnionService, PubSubProxy, DataIPCService and FerretProxy.

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
          ┌─────────────────────┼──────────────────────┐
          │                     │                      │
          ▼                     ▼                      ▼
     HyperSnap              QStorage                 QKMS
                                │
                     ┌──────────┴──────────┐
                     │                     │
                     ▼                     ▼
               Authenticated          Presigning
                 SigV4                  SigV4
                     │                     │
                     ▼                     ▼
              Backend requests       Temporary URLs
                                           │
                                           ▼
                              Browser/Desktop/Mobile
                                           │
                                           └──────► QStorage

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

The generic SigV4 implementation remains independent from QStorage path/object helpers, and the high-level storage facade does not expose signer internals.

---

## Module layout

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

Most applications need only:

```cpp
import quilibrium;
```

---

## Requirements

```text
CMake 3.30+
C++23 compiler
OpenSSL 3
libcurl
Ninja (recommended)
```

C++ Modules support depends on compiler/build-system versions.

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

Strict warnings:

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
ctest --test-dir build --output-on-failure
```

The deterministic presigning suite covers:

- official AWS S3 SigV4 presign test vector
- GET / PUT / HEAD
- fixed timestamps and deterministic signatures
- expiration validation
- credential scope
- canonical query sorting
- existing and duplicate query parameters
- empty S3 subresources
- spaces, UTF-8 and reserved object-key bytes
- session tokens
- required signed headers
- Content-Type-constrained PUT
- missing credentials
- arbitrary multipart-compatible query parameters
- configurable QStorage region
- secret-key non-disclosure
- tamper-sensitive signed header behavior

### Optional live QStorage integration test

Live tests are opt-in:

```bash
cmake \
    -S . \
    -B build-live \
    -G Ninja \
    -DQUILIBRIUM_ENABLE_LIVE_TESTS=ON

cmake --build build-live
```

Set:

```bash
export Q_ACCESS_KEY_ID="..."
export Q_SECRET_ACCESS_KEY="..."
export Q_STORAGE_BUCKET="..."
# Optional:
export Q_STORAGE_REGION="..."
export Q_STORAGE_ENDPOINT="https://qstorage.quilibrium.com"
```

Then:

```bash
ctest --test-dir build-live -R qstorage_presign_live --output-on-failure
```

The live test generates a presigned PUT, uploads without credentials, generates a presigned GET and verifies bytes, performs a presigned HEAD, and cleans up through the authenticated SDK path. It returns CTest skip code 77 when credentials or network access are unavailable.

---

## Examples

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

Run the presign example:

```bash
Q_ACCESS_KEY_ID="..." \
Q_SECRET_ACCESS_KEY="..." \
./build/quilibrium_qstorage_presign_example my-bucket image.jpg image/jpeg
```

It prints the PUT URL, every required signed header, and a GET URL.

---

## Install and consume

```bash
cmake --install build --prefix "$HOME/.local/quilibrium"
```

Consumer project:

```cmake
find_package(Quilibrium CONFIG REQUIRED)
target_link_libraries(my_application PRIVATE Quilibrium::SDK)
```

```cpp
import quilibrium;
```

Consumers do not need to know internal module filenames.

---

## Cross-language bindings

```text
bindings/
├── c/
├── python/
├── rust/
├── node/
└── dotnet/
```

The C++ implementation remains the canonical protocol/service core, with a stable C ABI for other language runtimes.

---

## Security

- Never commit or log QStorage/QKMS secret credentials.
- Never distribute secret credentials in desktop, mobile, browser, or Mini App binaries/bundles.
- Use least-privilege credentials on trusted services.
- Keep presigned expirations as short as the workflow reasonably allows.
- Treat `required_headers` as part of the signature contract; clients must send the returned values exactly.
- A presigned URL is a bearer capability until it expires. Protect it accordingly.
- Secret access keys and derived signing keys are never included in presigned URLs or error messages.

---

## Current service coverage

See [`docs/SERVICE_MATRIX.md`](docs/SERVICE_MATRIX.md) for the detailed service matrix and intentionally reserved future surfaces.

---

## Upstream compatibility

Quilibrium evolves quickly. Protocol-sensitive compatibility information is kept in:

```text
compat/upstreams.json
```

---

## Licensing

This project interfaces with upstream Quilibrium projects that may use different licenses. See [`LICENSE-NOTICE.md`](LICENSE-NOTICE.md) before redistributing upstream-derived implementation code.
