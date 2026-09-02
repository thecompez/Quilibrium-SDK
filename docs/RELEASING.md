# Cross-platform CI and releases

Quilibrium SDK uses GitHub Actions to build, test, package, and publish native releases for the six primary desktop/server targets.

## Supported release targets

| Platform | Architecture | GitHub runner | Release asset |
| --- | --- | --- | --- |
| Linux | x86-64 | `ubuntu-24.04` | `quilibrium-sdk-<version>-linux-x64.tar.gz` |
| Linux | ARM64 | `ubuntu-24.04-arm` | `quilibrium-sdk-<version>-linux-arm64.tar.gz` |
| macOS | Intel x86-64 | `macos-15-intel` | `quilibrium-sdk-<version>-macos-x64.tar.gz` |
| macOS | Apple Silicon ARM64 | `macos-15` | `quilibrium-sdk-<version>-macos-arm64.tar.gz` |
| Windows | x86-64 | `windows-2025` | `quilibrium-sdk-<version>-windows-x64.zip` |
| Windows | ARM64 | `windows-11-arm` | `quilibrium-sdk-<version>-windows-arm64.zip` |

The CI workflow runs the same platform matrix for pushes to `main`, pull requests, and manual CI runs.

## CI contract

Every platform job performs the following sequence:

1. Install or select the required compiler and native dependencies.
2. Configure the project in C++23 mode.
3. Build the SDK, examples, C ABI, and tests.
4. Run the deterministic CTest suite.
5. Install the SDK into a clean staging prefix.
6. Configure a separate consumer project with `find_package(Quilibrium CONFIG REQUIRED)`.
7. Build and run both the C++ module consumer and C ABI consumer.
8. Generate a CPack archive and verify that the archive is readable and non-empty.

A platform is not considered release-ready merely because the core library compiled.

## Dependencies

The C++ SDK requires:

- CMake 3.30 or newer
- a C++23 compiler with CMake C++ module support
- OpenSSL 3
- libcurl

Linux and macOS CI use native development packages. Windows CI uses vcpkg with static dependency triplets so the exported C ABI DLL does not require separate vcpkg OpenSSL/curl DLL deployment.

The installed CMake package intentionally declares OpenSSL and CURL as dependencies because C++ consumers link the static `Quilibrium::SDK` target.

## Creating a release

The release version is defined in the root `CMakeLists.txt`:

```cmake
project(quilibrium_cpp_sdk VERSION 1.1.0 LANGUAGES C CXX)
```

The Git tag must match that version exactly.

For version `1.2.0`:

```bash
# Update project(... VERSION 1.2.0 ...) first.
git add CMakeLists.txt
git commit -m "Prepare v1.2.0"
git push origin main

git tag -a v1.2.0 -m "Quilibrium SDK 1.2.0"
git push origin v1.2.0
```

Pushing the tag starts `.github/workflows/release.yml`.

The release workflow refuses to publish when the tag version and CMake project version differ.

## Manual release rerun

The release workflow can also be started manually from GitHub Actions. Supply an existing tag such as:

```text
v1.2.0
```

Manual mode still validates that the tag exists and that its version matches the CMake project version at the tagged commit.

## Published assets

A successful release contains exactly six platform archives plus two integrity metadata files:

```text
quilibrium-sdk-1.2.0-linux-x64.tar.gz
quilibrium-sdk-1.2.0-linux-arm64.tar.gz
quilibrium-sdk-1.2.0-macos-x64.tar.gz
quilibrium-sdk-1.2.0-macos-arm64.tar.gz
quilibrium-sdk-1.2.0-windows-x64.zip
quilibrium-sdk-1.2.0-windows-arm64.zip
SHA256SUMS
RELEASE-MANIFEST.json
```

`SHA256SUMS` is generated only after all six platform builds succeed.

`RELEASE-MANIFEST.json` records the release version, tag, source commit, artifact sizes, and SHA-256 hashes.

## Release package contents

CPack packages the installed SDK rather than the build tree. A release archive contains the platform-specific libraries together with the C API header, CMake package metadata, public C++ module interfaces, language bindings, examples, compatibility metadata, and project documentation.

The exact library file names vary by platform, but the package follows this shape:

```text
<package>/
├── bin/
├── include/
│   └── quilibrium.h
├── lib/
│   ├── cmake/Quilibrium/
│   └── quilibrium/modules/
├── bindings/
├── compat/
├── docs/
├── examples/
├── README.md
├── RELEASE-NOTES.md
└── LICENSE-NOTICE.md
```

On Windows the C ABI import library is installed into `lib/` and the DLL into `bin/`.

## Consuming the C++ package

After extracting or installing the package, point CMake at the installation prefix:

```bash
cmake \
    -S . \
    -B build \
    -DCMAKE_PREFIX_PATH=/path/to/quilibrium-sdk
```

Then use the exported target:

```cmake
find_package(Quilibrium CONFIG REQUIRED)
target_link_libraries(my_application PRIVATE Quilibrium::SDK)
```

```cpp
import quilibrium;
```

OpenSSL 3 and CURL development packages must also be discoverable by the consumer toolchain.

## Consuming the stable C ABI

C and language-binding consumers use the native shared library:

- Linux: `libquilibrium.so`
- macOS: `libquilibrium.dylib`
- Windows: `quilibrium.dll`

The public C header is installed as:

```text
include/quilibrium.h
```

Python, Rust, Node.js, and .NET wrapper sources are included under `bindings/` in every release package.

## Failure policy

The release is published only after all six platform jobs succeed. A failure in tests, package installation, consumer verification, or archive generation prevents the publish job from running.

Live QStorage integration tests remain opt-in because they require credentials. Release CI uses the deterministic offline/network-independent test suite and does not require repository secrets.
