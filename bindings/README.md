# Language bindings

The language bindings use the stable C ABI in `bindings/c`. The native library is named `quilibrium` (`libquilibrium.so`, `libquilibrium.dylib`, or `quilibrium.dll`).

## C configuration

`ql_sdk_config` accepts optional endpoints for HyperSnap, QStorage, QKMS, and Quilibrium native protocol RPC plus credentials used by the QStorage/QKMS bridge.

## Python

```bash
export QUILIBRIUM_SDK_LIB=/path/to/libquilibrium.so
export PYTHONPATH=/path/to/sdk/bindings/python
python3 - <<'PY'
from quilibrium_sdk import SDK

with SDK(hypersnap_endpoint="https://haatz.quilibrium.com") as q:
    response = q.user_by_fid(3)
    print(response.json())
PY
```

The release CTest suite loads the built native library through this wrapper and validates SDK creation/version.

## Rust

`bindings/rust` is a source FFI crate. Configure the linker using `QUILIBRIUM_SDK_LIB_DIR` as described in its README.

```rust
use quilibrium_sdk::{Config, Sdk};

let sdk = Sdk::new(Config {
    hypersnap_endpoint: Some("https://haatz.quilibrium.com".into()),
    ..Default::default()
})?;
let user = sdk.user_by_fid(3)?;
```

## Node.js

The source wrapper uses `koffi`:

```js
const { SDK } = require('./bindings/node');
const q = new SDK({ hypersnapEndpoint: 'https://haatz.quilibrium.com' });
const user = q.userByFid(3);
q.close();
```

## .NET

`bindings/dotnet/Quilibrium.cs` provides a P/Invoke wrapper:

```csharp
using var q = new Quilibrium.Sdk(hypersnapEndpoint: "https://haatz.quilibrium.com");
var user = q.UserByFid(3);
```

## Native protocol from bindings

Set `protocol_endpoint` / `protocolEndpoint` in the language-specific configuration before calling `native_call`. The payload must be the protobuf-encoded request body; the returned bytes are the protobuf-encoded response body after gRPC unframing.
