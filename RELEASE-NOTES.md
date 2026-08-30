# Quilibrium C++ SDK v1.1.0

## SigV4 presigned URLs

This release adds production-grade AWS Signature Version 4 query presigning as a generic SDK capability, with first-class QStorage support.

### Generic `quilibrium.sigv4`

- Added `auth::presign_options`.
- Added `auth::presigned_request` containing the final URL, caller-required signed headers, and expiration time.
- Added `sigv4_signer::presign(http_request, ...)` without changing the existing header-based `sign(http_request&)` API.
- Added expiration validation for the SigV4 1..604800-second range.
- Added S3-compatible `UNSIGNED-PAYLOAD` support and an explicit payload-mode extension point.
- Added canonical URI/query handling, existing/duplicate query preservation, empty subresources, UTF-8/reserved path encoding, session tokens, signed-header normalization, and conflicting `X-Amz-*` query rejection.

### QStorage

- Added `qstorage::client::presign_put_object()`.
- Added `qstorage::client::presign_get_object()`.
- Added `qstorage::client::presign_head_object()`.
- Added generic `qstorage::client::presign()` for arbitrary S3-compatible targets and future multipart presigning.
- Added a presign-only `qstorage::client(config)` constructor that does not require an HTTP transport.
- PUT Content-Type can be signed and is returned in `required_headers`.
- QStorage signing region is explicitly configurable.

### High-level SDK facade

- Added `storage().presign_put()`.
- Added `storage().presign_get()`.
- Added `storage().presign_head()`.
- Added `sdk_config::qstorage_region` while preserving existing authenticated storage operations.

### Tests and examples

- Added deterministic SigV4 presigning tests including the official AWS S3 query-auth vector.
- Added coverage for expiration, query sorting, duplicate parameters, UTF-8/reserved keys, session tokens, signed Content-Type, missing credentials, region configuration, and multipart-compatible query strings.
- Added `quilibrium_qstorage_presign_example`.
- Added optional credentialed QStorage PUT/GET/HEAD live integration test behind `QUILIBRIUM_ENABLE_LIVE_TESTS`.

### Portability

- Preserved the C++23 named-module architecture and install/export package.
- Implementation units explicitly include the standard-library declarations they use.
- Removed an unnecessary `std::string::reserve()` from `percent_encode()` after ASan exposed a GCC Modules TS sized-delete mismatch with long encoded paths in the validation environment.

Existing `sign()`, `storage().put()`, `storage().get()`, `storage().remove()`, `find_package(Quilibrium CONFIG REQUIRED)`, `Quilibrium::SDK`, and `import quilibrium;` usage remains source-compatible.
