# Python binding

Install the C++ SDK first, then point `QUILIBRIUM_SDK_LIB` at `libquilibrium.so`, `libquilibrium.dylib`, or `quilibrium.dll` when it is not on the platform loader path.

```python
from quilibrium_sdk import SDK

with SDK() as q:
    user = q.user_by_fid(3).json()["user"]
    print(user["username"])
```

QStorage/QKMS calls accept the same credentials passed to `SDK(...)`. `native_call()` accepts the numeric `native_service` value and raw protobuf payload bytes.
