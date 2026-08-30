use std::env;

fn main() {
    if let Ok(path) = env::var("QUILIBRIUM_LIB_DIR") {
        println!("cargo:rustc-link-search=native={path}");
    }
    println!("cargo:rustc-link-lib=dylib=quilibrium");
    println!("cargo:rerun-if-env-changed=QUILIBRIUM_LIB_DIR");
}
