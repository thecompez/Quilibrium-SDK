# Build verification — v1.0.1

## Portability regression fixed

The v1.0.0 JSON implementation unit relied on transitive visibility of standard-library facilities. GCC accepted it, while Clang/Apple Clang module builds correctly reported that `std::holds_alternative`, `std::get_if`, and `std::optional` were not visible in `src/json.cpp`.

v1.0.1 explicitly includes the implementation dependencies (`<variant>`, `<optional>`, `<map>`, and `<vector>`) and removes the signedness-conversion warnings in `core.cpp`, `json.cpp`, and `sigv4.cpp`.

## GNU build

Validated with GNU GCC 14.2.0, CMake/Ninja, C++23 modules, examples, C ABI, and tests enabled. The validation build used `QUILIBRIUM_WARNINGS_AS_ERRORS=ON`. GCC 14's known Modules TS `-Wattributes` BMI diagnostic is left as a warning because it originates while loading standard-library declarations rather than from SDK source.

Result: build succeeded.

CTest result:

```text
100% tests passed, 0 tests failed out of 6

quilibrium_core_tests ............. Passed
quilibrium_json_tests ............. Passed
quilibrium_facade_tests ........... Passed
quilibrium_service_tests .......... Passed
quilibrium_c_api_tests ............ Passed
quilibrium_python_binding_tests ... Passed
```

## Clang module validation

The available Clang toolchain in the validation container does not ship `clang-scan-deps`, so a CMake-driven Clang modules build cannot be generated there. Instead, all 17 public C++ module interfaces were explicitly precompiled with Clang 17 and all SDK implementation units were compiled against those BMIs with:

```text
-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
```

Result: passed.

This directly validates the Clang declaration-visibility failure that affected `json.cpp`. A native macOS/LLVM 22 build remains the appropriate platform-specific verification for Mach-O and Homebrew dependency paths.

## Install / consumer test

The SDK was installed to a clean prefix using `cmake --install`. A separate project then used:

```cmake
find_package(Quilibrium CONFIG REQUIRED)
target_link_libraries(consumer PRIVATE Quilibrium::SDK)
```

and:

```cpp
import quilibrium;
```

Result: configure, module BMI generation, build, link, and execution succeeded.

## Network note

Live public-peer testing depends on outbound DNS/network access. Network-independent transport, SigV4, QStorage, QKMS, HyperSnap, native gRPC framing, C ABI, and binding paths are covered by deterministic tests.
