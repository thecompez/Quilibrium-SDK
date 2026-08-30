# Quilibrium C++ SDK v1.0.1

## Portability fix

This patch fixes Clang/Apple Clang C++23 module builds where the JSON implementation unit relied on standard-library declarations that were visible transitively under GCC but not guaranteed to be visible in a module implementation unit. `src/json.cpp` now explicitly includes `<variant>`, `<optional>`, `<map>`, and `<vector>`.

The patch also removes signedness-conversion warnings in JSON escaping, URL percent encoding, and SigV4 whitespace normalization.

For GCC 14 Modules TS, `QUILIBRIUM_WARNINGS_AS_ERRORS=ON` now keeps SDK warnings fatal while exempting GCC's known compiler-internal `-Wattributes` BMI diagnostic from being promoted to an error.

## Validation

- Full GNU GCC 14.2 build: passed.
- CTest: 6/6 passed.
- Clang 17: every public module interface was manually precompiled and every SDK implementation unit was compiled with `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror`; passed.
- The exact `json.cpp` failure reported with Clang (`std::holds_alternative`, `std::get_if`, and `std::optional` not visible) is fixed.

## SDK surface

The release retains the v1.0 API: high-level `quilibrium::sdk`, HyperSnap reads, QStorage/S3-compatible operations and multipart upload, QKMS/TrentService operations, native unary gRPC framing and service registry, C ABI, Python/Rust/Node/.NET bindings, installable CMake package, and Qt/Mini App examples.

Streaming native RPCs remain registry/extension points; the built-in native transport is unary gRPC.
