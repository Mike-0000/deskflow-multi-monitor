mod commands;
mod config;
mod daemon;
mod displays;
mod ipc;
mod process;
mod tray;
mod version;

use commands::SharedCore;
use process::CoreProcess;
use std::sync::Arc;
use tauri::Manager;
use tokio::sync::Mutex;

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    let _ = env_logger::try_init();

    let core = Arc::new(Mutex::new(CoreProcess::new()));

    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .plugin(tauri_plugin_shell::init())
        .plugin(tauri_plugin_dialog::init())
        .plugin(tauri_plugin_process::init())
        .plugin(tauri_plugin_single_instance::init(|app, _args, _cwd| {
            if let Some(window) = app.get_webview_window("main") {
                let _ = window.show();
                let _ = window.set_focus();
            }
        }))
        .manage(core.clone())
        .setup(move |app| {
            let core_for_app = core.clone();
            let core_boot = core.clone();

            {
                let handle = app.handle().clone();
                tauri::async_runtime::spawn(async move {
                    let mut guard = core_for_app.lock().await;
                    guard.set_app(handle);
                });
            }

            if let Err(e) = tray::setup_tray(app.handle()) {
                log::warn!("tray setup failed: {e}");
            }

            // Attach to core already owned by the Windows service / a prior start,
            // then honor autoStartCore when nothing is running yet.
            tauri::async_runtime::spawn(async move {
                CoreProcess::attach_if_running(core_boot.clone()).await;
                let should_start = {
                    let guard = core_boot.lock().await;
                    guard.settings().auto_start_core
                        && matches!(guard.status.process_state, process::ProcessState::Stopped)
                        && guard.settings().core_mode != config::CoreMode::None
                };
                if should_start {
                    let _ = CoreProcess::start(core_boot).await;
                }
            });

            // Close-to-tray (default on; settings.closeToTray can be wired later for quit)
            if let Some(window) = app.get_webview_window("main") {
                let window_ref = window.clone();
                let core_for_close = app.state::<SharedCore>().inner().clone();
                window.on_window_event(move |event| {
                    if let tauri::WindowEvent::CloseRequested { api, .. } = event {
                        let close_to_tray = core_for_close
                            .try_lock()
                            .map(|g| g.settings().close_to_tray)
                            .unwrap_or(true);
                        if close_to_tray {
                            api.prevent_close();
                            let _ = window_ref.hide();
                        }
                    }
                });
            }

            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            commands::get_version,
            commands::get_status,
            commands::get_settings,
            commands::set_settings,
            commands::get_server_config,
            commands::set_server_config,
            commands::core_start,
            commands::core_stop,
            commands::core_restart,
            commands::trust_fingerprint,
            commands::add_screen,
            commands::ensure_minimal_layout,
            commands::get_local_ips,
            commands::daemon_available,
            commands::daemon_clear_settings,
            commands::daemon_log_path,
            commands::import_local_displays,
            commands::sync_layout_machines,
        ])
        .run(tauri::generate_context!())
        .expect("error while running Deskflow UI");
}
