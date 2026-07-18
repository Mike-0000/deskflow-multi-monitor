use serde::Deserialize;
use std::sync::OnceLock;

#[derive(Debug, Clone, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct VersionInfo {
    pub version: String,
    pub git_sha: String,
    pub version_id: String,
    pub core_ipc_name: String,
    pub daemon_ipc_name: String,
    pub gui_ipc_name: String,
    pub core_bin_name: String,
    pub app_name: String,
    pub app_id: String,
}

static VERSION: OnceLock<VersionInfo> = OnceLock::new();

pub fn info() -> &'static VersionInfo {
    VERSION.get_or_init(|| {
        let raw = include_str!("../version.json");
        serde_json::from_str(raw).expect("invalid version.json")
    })
}
