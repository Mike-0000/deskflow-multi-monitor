fn main() {
    // Do NOT regenerate version.json here. Writing a watched file from build.rs
    // causes an infinite rebuild loop with `tauri dev`.
    // version.json is produced by `npm run gen:version` (beforeBuildCommand / manual).
    println!("cargo:rerun-if-changed=version.json");
    tauri_build::build()
}
