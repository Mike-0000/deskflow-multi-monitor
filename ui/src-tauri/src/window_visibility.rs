use std::sync::atomic::{AtomicBool, Ordering};
use tauri::{AppHandle, Emitter, Manager, Runtime, WebviewWindow};

/// Tracks whether the main window is shown. When false, skip waking the WebView
/// with high-frequency log events (close-to-tray still keeps the WebView alive).
static WINDOW_VISIBLE: AtomicBool = AtomicBool::new(true);

pub fn is_visible() -> bool {
    WINDOW_VISIBLE.load(Ordering::Relaxed)
}

pub fn set_visible<R: Runtime>(app: &AppHandle<R>, visible: bool) {
    WINDOW_VISIBLE.store(visible, Ordering::Relaxed);
    let _ = app.emit("deskflow-window-visible", visible);
}

pub fn show_main<R: Runtime>(app: &AppHandle<R>) {
    if let Some(window) = app.get_webview_window("main") {
        show_window(app, &window);
    }
}

pub fn show_window<R: Runtime>(app: &AppHandle<R>, window: &WebviewWindow<R>) {
    let _ = window.show();
    let _ = window.set_focus();
    set_visible(app, true);
}

pub fn hide_window<R: Runtime>(app: &AppHandle<R>, window: &WebviewWindow<R>) {
    let _ = window.hide();
    set_visible(app, false);
}
