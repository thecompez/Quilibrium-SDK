# Architecture

## Layering

The SDK is intentionally split into a stable application facade and power-user protocol/service modules.

```text
Native / Qt / Service / Mini-app backend
                 |
          quilibrium::sdk
                 |
   +-------------+----------------+-----------------+
   |             |                |                 |
HyperSnap     QStorage          QKMS         Native unary RPC
HTTP/JSON     S3 + SigV4        KMS + SigV4  gRPC framing + protobuf bytes
   |             |                |                 |
   +-------------+----------------+-----------------+
                 |
         http_transport
                 |
       libcurl HTTP/TLS
```

Power-user modules remain separately importable:

```text
quilibrium.protocol   exact service/method registry + RPC abstraction
quilibrium.qstorage   low-level S3 client + multipart + raw signed execute
quilibrium.qkms       low-level KMS operation surface + generic invoke
quilibrium.hypersnap  low-level v2 HTTP client
quilibrium.crypto     protocol-crypto provider boundary
quilibrium.token      native token domain model
quilibrium.hypergraph native hypergraph domain model
quilibrium.compute    compute/intrinsic domain model
quilibrium.farcaster  signed-message writer boundary
```

## Public ABI boundaries

1. Generated protobuf types are not exposed by the public facade.
2. The facade stores an opaque reference-counted state handle rather than low-level module client objects.
3. HTTP transport injection uses a synchronous function-pointer/opaque-context boundary internally; coroutine `task<T>` is an API wrapper rather than a cross-module transport ABI.
4. This arrangement avoids GCC 14 C++ Modules runtime/ABI instability observed with coroutine and STL ownership objects crossing named-module boundaries.
5. C ABI bindings never expose C++ STL or coroutine types.

## Async model

Public service calls return the SDK's eager `task<T>` type. Native coroutine-aware application code can `co_await` it; ordinary CLI/Qt/service code can use `sync_wait()`.

The default libcurl transport is currently blocking at the transport boundary. Applications requiring event-loop integration can inject their own `http_transport` callback while preserving the same public service API.

## Routing and failover

The high-level facade keeps an independent route pool per service family. Each route tracks:

- EWMA latency;
- consecutive failure count;
- temporary quarantine;
- retryable transport/service errors.

Reads and replay-safe calls can fail over according to `call_options`. Applications should treat non-idempotent writes carefully and tune attempts/failover according to the operation's replay semantics.

Low-level QStorage/QKMS clients use deterministic endpoint rotation and retry logic without depending on `quilibrium.net` across a module ABI boundary.

## QStorage

QStorage requests are canonicalized and signed using AWS Signature Version 4 with service `s3`. The low-level module exposes common S3 object/bucket operations, multipart operations, and a raw signed request escape hatch for other supported S3 subresources/configuration APIs.

## QKMS

QKMS uses AWS-style SigV4 with service `kms` and `x-amz-target: TrentService.<Operation>`. Named convenience functions are thin wrappers over `invoke()`, keeping new compatible operations available before a typed convenience method is added.

## Native protocol

The high-level native path maps `native_service` to the exact protobuf package/service name, creates standard unary gRPC framing, sends it through HTTP, validates gRPC status, and returns the unframed protobuf response payload.

Streaming method metadata is represented in `quilibrium.protocol`, but streaming transport execution is intentionally outside the v1.0 high-level implementation.

## Cryptography

OpenSSL provides conventional primitives needed by this SDK (SHA/HMAC and SigV4-related operations). Quilibrium-specific BLS48-581, range/traversal proofs, VDF, verifiable encryption, and MPC behavior stays behind `crypto::protocol_provider` or application-specific codec/provider implementations.

This keeps unusual cryptography replaceable and avoids binding the public SDK ABI to a specific external implementation or licensing model.

## Language bindings

```text
C++23 modules
      |
 stable C ABI (libquilibrium)
      |
 +----+---------+----------+----------+
Python        Rust       Node.js     .NET
ctypes        FFI        Koffi       P/Invoke
```

The C ABI owns all buffers it returns and provides explicit response/error free functions. Language wrappers copy response data into language-owned memory before releasing native buffers.


## Presigned QStorage data path

SigV4 query presigning is intentionally split across three layers:

```text
quilibrium.sigv4       generic canonical request + query authentication
        ↓
quilibrium.qstorage    S3/QStorage object paths and signed Content-Type contract
        ↓
quilibrium.sdk         storage().presign_put/get/head facade
```

This allows trusted services to issue short-lived object capabilities while untrusted browser, desktop, or mobile clients transfer object bytes directly to QStorage. Credentials remain owned by the trusted signer/client configuration and do not cross into the distributed client.
