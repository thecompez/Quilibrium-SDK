# Mini-app / web backend pattern

Recommended topology:

```text
Browser / Farcaster Mini App
          |
       HTTPS
          |
Your backend (C++/Rust/Node/etc.)
          |
 Quilibrium C++ SDK
    |      |      |
HyperSnap QStorage QKMS
```

Do **not** put QStorage/QKMS access keys or threshold-wallet/session secrets into browser JavaScript.

`handler.cpp` is framework-neutral and shows how a backend route can proxy a HyperSnap resource through a long-lived `quilibrium::sdk`. In a real service, create the SDK once during application startup and inject it into route handlers.

A frontend can then use an ordinary endpoint such as:

```ts
const response = await fetch(`/api/farcaster/users/${fid}`);
const user = await response.json();
```

For write operations, keep signing/key authorization on the trusted side or use a deliberate user-wallet signature flow. HyperSnap read APIs are not a substitute for signed Farcaster protocol writes.
