# Build verification — v1.1.0

## Scope

This verification covers the v1.1.0 production SigV4 presigned URL implementation, QStorage integration, high-level facade, deterministic tests, optional live-test target, examples, package install/export compatibility, and C++ module portability checks.

## Presigning correctness

The deterministic `quilibrium_presign_tests` suite passes an official AWS S3 Signature Version 4 query-authentication vector with fixed credentials, timestamp, expiration, canonical request, credential scope, and expected final signature.

Additional deterministic coverage includes:

- GET, PUT, and HEAD presigning.
- `X-Amz-Algorithm`, `X-Amz-Credential`, `X-Amz-Date`, `X-Amz-Expires`, `X-Amz-SignedHeaders`, and `X-Amz-Signature`.
- `X-Amz-Security-Token` for session credentials.
- S3 `UNSIGNED-PAYLOAD` behavior.
- 1..604800-second expiration validation with no clamping.
- Existing query parameters, duplicate parameters, empty subresources, and canonical sorting.
- Already-percent-encoded query values without double encoding.
- Object keys containing spaces, UTF-8 bytes, slash-delimited paths, and reserved URL characters.
- Signed `content-type` contract for PUT and explicit `required_headers`.
- Signature changes when a signed Content-Type changes.
- Missing credentials and missing requested signed-header failures.
- Secret-key non-disclosure in the final URL.
- Arbitrary multipart-compatible query targets such as `partNumber` and `uploadId`.
- Multiple QStorage endpoints and explicit signing-region configuration.
- High-level `storage().presign_put/get/head` facade integration.

## GNU build

Validated in the available build environment with GNU GCC 14.2.0, CMake/Ninja, C++23 modules, examples, C ABI, and tests enabled.

The strict build used:

```text
-Wall -Wextra -Wpedantic -Wconversion -Wshadow
QUILIBRIUM_WARNINGS_AS_ERRORS=ON
```

GCC 14's known Modules TS `-Wattributes` BMI diagnostic is kept from being promoted to an error because it originates while loading standard-library BMI declarations rather than SDK source.

Result: **passed**.

CTest result:

```text
100% tests passed, 0 tests failed out of 7

quilibrium_core_tests ............. Passed
quilibrium_json_tests ............. Passed
quilibrium_facade_tests ........... Passed
quilibrium_service_tests .......... Passed
quilibrium_presign_tests .......... Passed
quilibrium_c_api_tests ............ Passed
quilibrium_python_binding_tests ... Passed
```


### GCC 14 Release-test optimizer note

GCC 14 Modules TS triggers an internal compiler error in `ipa-comdats` when the large deterministic `quilibrium_presign_tests` module consumer is optimized at Release `-O3`. This is a compiler failure rather than a source diagnostic. CMake therefore lowers **only that test executable** to `-O1` for GNU compilers older than GCC 15. The SDK library and examples remain at the selected Release optimization level. This workaround is intentionally scoped and documented.

## AddressSanitizer

The presigning test suite was also executed under AddressSanitizer while investigating C++ module lifetime/allocator interactions uncovered by long UTF-8/percent-encoded paths.

A pre-existing `percent_encode()` optimization using `std::string::reserve()` triggered a GCC Modules TS sized-delete mismatch for longer strings in that environment. The unnecessary reserve was removed without changing encoding semantics.

Result after the fix: **presigning ASan test passed**.

## Clang module validation

The validation container provides Clang 17 but does not provide `clang-scan-deps`, so a full CMake-driven Clang modules build cannot be generated there.

Instead:

- all 17 public module interfaces were explicitly precompiled with Clang 17;
- all SDK implementation units were compiled against those BMIs;
- `tests/presign_tests.cpp`, `tests/qstorage_presign_live.cpp`, and `examples/qstorage_presign.cpp` were compiled as consumers;
- compilation used `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror`.

Result: **passed**.

This check is specifically intended to catch implementation-unit reliance on transitive standard-library visibility.

### Not validated in this environment

- Homebrew LLVM/Clang 22 on macOS was **not available** in the validation container and is therefore not claimed as passed here.
- Apple Clang/Mach-O-specific linking was **not validated** here.

## Install / package consumer test

The SDK was installed to a clean prefix using `cmake --install`.
A separate clean consumer project then used:

```cmake
find_package(Quilibrium CONFIG REQUIRED)
target_link_libraries(consumer PRIVATE Quilibrium::SDK)
```

and:

```cpp
import quilibrium;
```

The consumer configured, generated module BMIs, built, linked, executed, and generated a presigned URL without network I/O.

Result: **passed**.

## Optional live QStorage test

`quilibrium_qstorage_presign_live_tests` is only registered when:

```text
QUILIBRIUM_ENABLE_LIVE_TESTS=ON
```

It requires `Q_ACCESS_KEY_ID`, `Q_SECRET_ACCESS_KEY`, and `Q_STORAGE_BUCKET`, with optional `Q_STORAGE_REGION` and `Q_STORAGE_ENDPOINT`.

The test performs:

1. presigned PUT generation;
2. direct credential-free HTTP upload;
3. presigned GET generation and byte verification;
4. presigned HEAD;
5. cleanup through authenticated SDK access.

When credentials or usable network access are unavailable it exits with code 77 and is reported by CTest as skipped.

A credentialed live QStorage run was **not performed in the validation environment**, because no QStorage credentials were available. No claim of live service compatibility is made beyond the deterministic SigV4/AWS vector and network-independent integration tests.

## Secret review

The release sources/tests/examples contain only deliberate test-vector credentials (including the public AWS documentation example) and placeholders. No live QStorage credentials are included.
