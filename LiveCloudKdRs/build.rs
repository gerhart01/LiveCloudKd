fn main() {
    // hvlib.lib lives next to LiveCloudKdRs in LiveCloudKdSdk/files/
    let manifest_dir = std::env::var("CARGO_MANIFEST_DIR").unwrap();
    let lib_path = format!("{}\\..\\LiveCloudKdSdk\\files", manifest_dir);
    println!("cargo:rustc-link-search=native={}", lib_path);
    println!("cargo:rustc-link-lib=dylib=hvlib");
    // Re-run if the import library changes
    println!("cargo:rerun-if-changed={}\\hvlib.lib", lib_path);
}
