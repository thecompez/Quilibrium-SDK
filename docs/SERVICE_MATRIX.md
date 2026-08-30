# Quilibrium C++ SDK Service Matrix — v1.1.0

| Area | Module / facade | v1.1 coverage | Boundary |
|---|---|---|---|
| HyperSnap users | `quilibrium.sdk`, `quilibrium.hypersnap` | Typed FID/username/search + raw GET/POST | HTTP write endpoints that upstream returns as 501 are not treated as Farcaster writes |
| HyperSnap casts | `quilibrium.sdk`, `quilibrium.hypersnap` | Typed get/search, conversation JSON + raw API | Farcaster writes use signed protocol messages |
| HyperSnap feeds | `quilibrium.sdk`, `quilibrium.hypersnap` | Following/trending/user-casts typed pages | Additional documented paths remain available through raw GET |
| QStorage | `quilibrium.sdk`, `quilibrium.qstorage` | Header SigV4, presigned PUT/GET/HEAD URLs, signed-header contracts, bucket/object common ops, copy/head/list, multipart create/upload/complete/abort/list, raw execute/presign | XML is returned raw; full presigned multipart orchestration can build on generic arbitrary-target presigning |
| QKMS | `quilibrium.sdk`, `quilibrium.qkms` | SigV4 generic invoke + named key/crypto/data-key/MAC/import/policy/alias/grant/tag/rotation/replication/deletion methods | QNZM/MPC sidecar login/session workflow is separate from the KMS-compatible API |
| NodeService | `quilibrium.sdk`, `quilibrium.protocol` | Unary raw-protobuf call + complete registry | Typed generated protobuf wrapper not bundled |
| ConnectivityService | same | Unary raw-protobuf + registry | — |
| GlobalService | same | Unary methods callable; stream method registered | Server streaming execution not implemented in v1.1 |
| AppShardService | same | Unary raw-protobuf + registry | — |
| HypergraphComparisonService | same | Unary method callable; streaming methods registered | Bidirectional streaming execution not implemented |
| KeyRegistryService | same | Unary raw-protobuf + registry | Cryptographic key/proof construction remains provider-specific |
| DispatchService | same | Unary raw-protobuf + registry | — |
| MixnetService | same | Unary `PutMessage`; `RoundStream` registered | Bidirectional streaming execution not implemented |
| OnionService | same | Method registered | Bidirectional streaming execution not implemented |
| PubSubProxy | same | Unary methods callable; streaming methods registered | Subscription/validator streams need streaming transport |
| DataIPCService | same | Unary raw-protobuf + registry | Operator/internal surface |
| FerretProxy | same | Current `AliceProxy`/`BobProxy` registry | Both are bidirectional streams; registry only in v1.1 |
| Token intrinsic | `quilibrium.token` | Strongly typed domain model | Canonical protobuf/message builders and proof/signing backend are extension work |
| Hypergraph intrinsic | `quilibrium.hypergraph` | Strongly typed domain model | Commitment/proof/signing builders require crypto/provider integration |
| Compute intrinsic | `quilibrium.compute` | Strongly typed domain model | Execution/intrinsic codecs remain provider/protocol-version specific |
| Farcaster writes | `quilibrium.farcaster` + native call | Signed-message writer boundary and raw submission transport | User signer/key management and protobuf message construction are intentionally not implicit |
| Protocol crypto | `quilibrium.crypto` | Replaceable provider contract | BLS48-581/range/traversal/VDF/MPC backend is not bundled |
| C ABI | `libquilibrium` | Create/destroy, version, HyperSnap, QStorage, QKMS, native unary call | Stable binary bridge for non-C++ languages |
| Python | `bindings/python` | C ABI wrapper; release-tested | — |
| Rust | `bindings/rust` | FFI wrapper source | Toolchain not available in release sandbox for execution test |
| Node.js | `bindings/node` | Koffi wrapper source | Koffi dependency not installed in release sandbox for execution test |
| .NET | `bindings/dotnet` | P/Invoke wrapper source | .NET SDK not installed in release sandbox for execution test |
| Klearu / MetaVM | reserved | No unstable API invented | Add only against pinned public contracts |
| QQ / QPing / later roadmap services | reserved | No stable API assumption | Add after public endpoint/contracts are available |

## QStorage coverage model

### Presigned URLs

QStorage supports first-class `presign_put`, `presign_get`, and `presign_head` APIs through the high-level facade, plus low-level object helpers and arbitrary-target presigning in `quilibrium.qstorage`. The generic signer preserves existing and duplicate query parameters, so multipart `uploads`, `partNumber`, and `uploadId` flows can be added without redesigning the SigV4 layer. Signed caller headers are returned explicitly with the URL.


The raw signed `execute()` function is deliberate. S3-compatible APIs contain many independent configuration subresources (ACL, lifecycle, CORS, website, encryption, policy, tagging, versioning, logging, ownership controls, public-access block, object lock, and related configuration calls). Common data-path and multipart calls are named; less common configuration operations can be issued without waiting for a new SDK release.

## QKMS coverage model

`invoke(operation, payload)` maps directly to `TrentService.<Operation>`. Named methods cover the operation families observed in the pinned official documentation. The generic operation path keeps the SDK forward-compatible with compatible operations introduced upstream.
