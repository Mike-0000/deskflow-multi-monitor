use crate::config::{AppSettings, MachineLayout, ServerConfig};
use crate::process::{CoreProcess, CoreStatus};
use crate::version;
use serde::Serialize;
use std::sync::Arc;
use tauri::State;
use tokio::sync::Mutex;

pub type SharedCore = Arc<Mutex<CoreProcess>>;

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct VersionPayload {
    pub version: String,
    pub git_sha: String,
    pub version_id: String,
    pub app_name: String,
}

#[tauri::command]
pub fn get_version() -> VersionPayload {
    let v = version::info();
    VersionPayload {
        version: v.version.clone(),
        git_sha: v.git_sha.clone(),
        version_id: v.version_id.clone(),
        app_name: v.app_name.clone(),
    }
}

#[tauri::command]
pub async fn get_status(core: State<'_, SharedCore>) -> Result<CoreStatus, String> {
    let guard = core.lock().await;
    Ok(guard.status.clone())
}

#[tauri::command]
pub async fn get_settings(core: State<'_, SharedCore>) -> Result<AppSettings, String> {
    let guard = core.lock().await;
    Ok(guard.settings().clone())
}

#[tauri::command]
pub async fn set_settings(
    core: State<'_, SharedCore>,
    settings: AppSettings,
) -> Result<(), String> {
    CoreProcess::apply_settings(core.inner().clone(), settings).await
}

#[tauri::command]
pub async fn get_server_config(core: State<'_, SharedCore>) -> Result<ServerConfig, String> {
    let guard = core.lock().await;
    Ok(guard.server_config().clone())
}

#[tauri::command]
pub async fn set_server_config(
    core: State<'_, SharedCore>,
    config: ServerConfig,
) -> Result<String, String> {
    let mut guard = core.lock().await;
    *guard.server_config_mut() = config;
    let path = guard.persist_server_config()?;
    Ok(path.display().to_string())
}

#[tauri::command]
pub async fn core_start(core: State<'_, SharedCore>) -> Result<(), String> {
    CoreProcess::start(core.inner().clone()).await
}

#[tauri::command]
pub async fn core_stop(core: State<'_, SharedCore>) -> Result<(), String> {
    CoreProcess::stop(core.inner().clone()).await
}

#[tauri::command]
pub async fn core_pause(core: State<'_, SharedCore>) -> Result<(), String> {
    CoreProcess::pause(core.inner().clone()).await
}

#[tauri::command]
pub async fn core_resume(core: State<'_, SharedCore>) -> Result<(), String> {
    CoreProcess::resume(core.inner().clone()).await
}

#[tauri::command]
pub async fn core_restart(core: State<'_, SharedCore>) -> Result<(), String> {
    CoreProcess::restart(core.inner().clone()).await
}

#[tauri::command]
pub async fn trust_fingerprint(
    core: State<'_, SharedCore>,
    fingerprint: String,
    as_server: bool,
) -> Result<(), String> {
    let guard = core.lock().await;
    guard
        .settings_store()
        .trust_fingerprint(&fingerprint, as_server)
}

#[tauri::command]
pub async fn add_screen(core: State<'_, SharedCore>, name: String) -> Result<ServerConfig, String> {
    let mut guard = core.lock().await;
    if !guard.server_config_mut().add_screen_name(&name) {
        return Err("could not add screen (duplicate or no free cell)".into());
    }
    guard.persist_server_config()?;
    Ok(guard.server_config().clone())
}

#[tauri::command]
pub async fn ensure_minimal_layout(
    core: State<'_, SharedCore>,
    client_name: String,
) -> Result<ServerConfig, String> {
    let mut guard = core.lock().await;
    guard
        .server_config_mut()
        .ensure_minimal_two_screen_layout(&client_name);
    guard.persist_server_config()?;
    Ok(guard.server_config().clone())
}

#[tauri::command]
pub async fn get_local_ips() -> Result<Vec<String>, String> {
    Ok(crate::process::local_ips_public())
}

#[tauri::command]
pub async fn daemon_available() -> Result<bool, String> {
    match crate::daemon::DaemonClient::connect().await {
        Ok(_) => Ok(true),
        Err(_) => Ok(false),
    }
}

#[tauri::command]
pub async fn daemon_clear_settings() -> Result<(), String> {
    let mut daemon = crate::daemon::DaemonClient::connect()
        .await
        .map_err(|e| e.to_string())?;
    daemon
        .send_clear_settings()
        .await
        .map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn daemon_log_path() -> Result<(), String> {
    let mut daemon = crate::daemon::DaemonClient::connect()
        .await
        .map_err(|e| e.to_string())?;
    daemon.request_log_path().await.map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn import_local_displays(
    core: State<'_, SharedCore>,
    machine_name: String,
) -> Result<ServerConfig, String> {
    let displays = crate::displays::enumerate_local_displays()?;
    let mut guard = core.lock().await;
    let layout = guard.server_config_mut().workspace_layout_mut();
    layout.enabled = true;
    layout.version = 2;

    let machine = if let Some(m) = layout.machines.iter_mut().find(|m| m.name == machine_name) {
        m
    } else {
        layout.machines.push(MachineLayout {
            name: machine_name.clone(),
            monitors: vec![],
        });
        layout
            .machines
            .iter_mut()
            .find(|m| m.name == machine_name)
            .unwrap()
    };
    machine.monitors = displays;
    guard.persist_server_config()?;
    Ok(guard.server_config().clone())
}

#[tauri::command]
pub async fn sync_layout_machines(core: State<'_, SharedCore>) -> Result<ServerConfig, String> {
    let mut guard = core.lock().await;
    // Called when turning on advanced layout; keep the flag set across persist/reload.
    {
        let layout = guard.server_config_mut().workspace_layout_mut();
        layout.enabled = true;
        layout.version = layout.version.max(2);
    }
    guard.server_config_mut().sync_machines_from_screens();
    guard.persist_server_config()?;
    Ok(guard.server_config().clone())
}
