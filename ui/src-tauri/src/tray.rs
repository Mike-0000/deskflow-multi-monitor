use crate::commands::SharedCore;
use crate::process::CoreProcess;
use tauri::{
    menu::{Menu, MenuItem},
    tray::{MouseButton, MouseButtonState, TrayIconBuilder, TrayIconEvent},
    AppHandle, Manager, Runtime,
};

pub fn setup_tray<R: Runtime>(app: &AppHandle<R>) -> tauri::Result<()> {
    let show_i = MenuItem::with_id(app, "show", "Show Deskflow", true, None::<&str>)?;
    let start_i = MenuItem::with_id(app, "start", "Start", true, None::<&str>)?;
    let stop_i = MenuItem::with_id(app, "stop", "Stop", true, None::<&str>)?;
    let quit_i = MenuItem::with_id(app, "quit", "Quit", true, None::<&str>)?;
    let menu = Menu::with_items(app, &[&show_i, &start_i, &stop_i, &quit_i])?;

    let tray = TrayIconBuilder::new()
        .icon(app.default_window_icon().unwrap().clone())
        .menu(&menu)
        .show_menu_on_left_click(false)
        .tooltip("Deskflow")
        .on_menu_event(|app, event| {
            match event.id.as_ref() {
                "show" => {
                    if let Some(window) = app.get_webview_window("main") {
                        let _ = window.show();
                        let _ = window.set_focus();
                    }
                }
                "quit" => {
                    app.exit(0);
                }
                "start" => {
                    let handle = app.clone();
                    tauri::async_runtime::spawn(async move {
                        if let Some(core) = handle.try_state::<SharedCore>() {
                            let _ = CoreProcess::start(core.inner().clone()).await;
                        }
                    });
                }
                "stop" => {
                    let handle = app.clone();
                    tauri::async_runtime::spawn(async move {
                        if let Some(core) = handle.try_state::<SharedCore>() {
                            let _ = CoreProcess::stop(core.inner().clone()).await;
                        }
                    });
                }
                _ => {}
            }
        })
        .on_tray_icon_event(|tray, event| {
            if let TrayIconEvent::Click {
                button: MouseButton::Left,
                button_state: MouseButtonState::Up,
                ..
            } = event
            {
                let app = tray.app_handle();
                if let Some(window) = app.get_webview_window("main") {
                    let _ = window.show();
                    let _ = window.set_focus();
                }
            }
        })
        .build(app)?;

    // Keep an explicit managed reference so the icon cannot be GC'd early.
    app.manage(tray);

    Ok(())
}
