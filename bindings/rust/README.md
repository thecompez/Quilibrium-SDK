# Rust binding

Install the C++ SDK shared library and set `QUILIBRIUM_LIB_DIR` when Cargo cannot find it automatically.

```rust
use quilibrium_sdk::{Config, Sdk};

let q = Sdk::new(Config::default())?;
let user = q.user_by_fid(3)?;
println!("{}", String::from_utf8_lossy(&user.body));
```
