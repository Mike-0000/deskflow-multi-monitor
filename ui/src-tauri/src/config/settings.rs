use crate::version;
use ini::Ini;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::fs;
use std::path::{Path, PathBuf};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize, Default)]
#[serde(rename_all = "lowercase")]
pub enum CoreMode {
    #[default]
    None,
    Client,
    Server,
}

impl CoreMode {
    pub fn as_ini(&self) -> &'static str {
        match self {
            CoreMode::None => "0",
            CoreMode::Client => "1",
            CoreMode::Server => "2",
        }
    }

    pub fn from_ini(value: &str) -> Self {
        match value.trim() {
            "1" | "client" | "Client" => CoreMode::Client,
            "2" | "server" | "Server" => CoreMode::Server,
            _ => CoreMode::None,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize, Default)]
#[serde(rename_all = "lowercase")]
pub enum ProcessMode {
    #[default]
    Desktop,
    Service,
}

impl ProcessMode {
    pub fn as_ini(&self) -> &'static str {
        match self {
            ProcessMode::Service => "0",
            ProcessMode::Desktop => "1",
        }
    }

    pub fn from_ini(value: &str) -> Self {
        match value.trim() {
            "0" | "service" | "Service" => ProcessMode::Service,
            _ => ProcessMode::Desktop,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct AppSettings {
    pub core_mode: CoreMode,
    pub process_mode: ProcessMode,
    pub remote_host: String,
    pub computer_name: String,
    pub port: u16,
    pub interface: String,
    pub log_level: String,
    pub close_to_tray: bool,
    pub auto_start_core: bool,
    pub tls_enabled: bool,
    pub check_peers: bool,
    pub elevate: bool,
    pub prevent_sleep: bool,
    pub language_sync: bool,
    pub invert_y_scroll: bool,
    pub invert_x_scroll: bool,
    pub y_scroll_scale: f64,
    pub x_scroll_scale: f64,
    pub dynamic_connection_retry: bool,
    pub external_config: bool,
    pub external_config_file: String,
    pub protocol: String,
    pub heartbeat: i32,
    pub has_heartbeat: bool,
    pub relative_mouse_moves: bool,
    pub switch_delay: i32,
    pub has_switch_delay: bool,
    pub clipboard_sharing: bool,
    pub clipboard_sharing_size: i32,
    pub raw: HashMap<String, String>,
}

impl Default for AppSettings {
    fn default() -> Self {
        let hostname = hostname::get()
            .ok()
            .and_then(|h| h.into_string().ok())
            .unwrap_or_else(|| "screen".into());
        Self {
            core_mode: CoreMode::None,
            process_mode: default_process_mode(),
            remote_host: String::new(),
            computer_name: hostname,
            port: 24800,
            interface: String::new(),
            log_level: "INFO".into(),
            close_to_tray: true,
            // Desktop mode is useless if the tray launches but never starts core.
            auto_start_core: true,
            tls_enabled: true,
            check_peers: true,
            elevate: cfg!(windows),
            prevent_sleep: false,
            language_sync: true,
            invert_y_scroll: false,
            invert_x_scroll: false,
            y_scroll_scale: 1.0,
            x_scroll_scale: 1.0,
            dynamic_connection_retry: true,
            external_config: false,
            external_config_file: String::new(),
            protocol: "Barrier".into(),
            heartbeat: 5000,
            has_heartbeat: false,
            relative_mouse_moves: false,
            switch_delay: 0,
            has_switch_delay: false,
            clipboard_sharing: true,
            clipboard_sharing_size: 1024 * 1024 * 10,
            raw: HashMap::new(),
        }
    }
}

fn default_process_mode() -> ProcessMode {
    // Tauri UI defaults to Desktop so first-run works without the Windows service.
    // Existing Deskflow.conf values still win when present (including Service).
    ProcessMode::Desktop
}

pub struct SettingsStore {
    path: PathBuf,
}

impl SettingsStore {
    pub fn new() -> Self {
        Self {
            path: Self::settings_file_path(),
        }
    }

    pub fn settings_file_path() -> PathBuf {
        if let Some(portable) = Self::portable_settings_file() {
            if portable.exists() {
                return portable;
            }
        }
        Self::user_settings_file()
    }

    pub fn user_dir() -> PathBuf {
        let app = &version::info().app_name;
        #[cfg(windows)]
        {
            dirs::home_dir()
                .unwrap_or_else(|| PathBuf::from("."))
                .join("AppData")
                .join("Roaming")
                .join(app)
        }
        #[cfg(target_os = "macos")]
        {
            dirs::home_dir()
                .unwrap_or_else(|| PathBuf::from("."))
                .join("Library")
                .join(app)
        }
        #[cfg(all(unix, not(target_os = "macos")))]
        {
            dirs::home_dir()
                .unwrap_or_else(|| PathBuf::from("."))
                .join(".config")
                .join(app)
        }
    }

    pub fn system_dir() -> PathBuf {
        let app = &version::info().app_name;
        #[cfg(windows)]
        {
            PathBuf::from(r"C:\ProgramData").join(app)
        }
        #[cfg(target_os = "macos")]
        {
            PathBuf::from("/Library").join(app)
        }
        #[cfg(all(unix, not(target_os = "macos")))]
        {
            PathBuf::from("/etc").join(app)
        }
    }

    pub fn user_settings_file() -> PathBuf {
        Self::user_dir().join(format!("{}.conf", version::info().app_name))
    }

    pub fn portable_settings_file() -> Option<PathBuf> {
        std::env::current_exe().ok().and_then(|exe| {
            exe.parent()
                .map(|p| p.join("settings").join(format!("{}.conf", version::info().app_name)))
        })
    }

    pub fn is_portable_mode() -> bool {
        Self::portable_settings_file()
            .map(|p| p.exists())
            .unwrap_or(false)
    }

    pub fn settings_path(&self) -> PathBuf {
        #[cfg(windows)]
        {
            if !Self::is_portable_mode() {
                return Self::system_dir();
            }
        }
        self.path
            .parent()
            .map(|p| p.to_path_buf())
            .unwrap_or_else(|| Self::user_dir())
    }

    pub fn server_config_file(&self, settings: &AppSettings) -> PathBuf {
        if settings.external_config && !settings.external_config_file.is_empty() {
            return PathBuf::from(&settings.external_config_file);
        }
        self.settings_path()
            .join(format!("{}-server.conf", version::info().app_id))
    }

    pub fn tls_dir(&self) -> PathBuf {
        self.settings_path().join("tls")
    }

    pub fn tls_trusted_servers_db(&self) -> PathBuf {
        self.tls_dir().join("trusted-servers")
    }

    pub fn tls_trusted_clients_db(&self) -> PathBuf {
        self.tls_dir().join("trusted-clients")
    }

    pub fn daemon_log_file(&self) -> PathBuf {
        self.settings_path()
            .join(format!("{}-daemon.log", version::info().app_id))
    }

    pub fn load(&self) -> AppSettings {
        let mut settings = AppSettings::default();
        if !self.path.exists() {
            return settings;
        }
        let Ok(ini) = Ini::load_from_file(&self.path) else {
            return settings;
        };

        let get = |key: &str| -> Option<String> {
            // QSettings IniFormat often stores as General/key or flat key
            for section in ini.sections() {
                if let Some(v) = ini.get_from(section, key) {
                    return Some(v.to_string());
                }
            }
            // Also try with path-style keys used by Qt (client/remoteHost)
            None
        };

        // Qt stores keys like "client/remoteHost" in the General section or as nested.
        let mut raw = HashMap::new();
        for (section, props) in ini.iter() {
            for (k, v) in props.iter() {
                let full = match section {
                    None | Some("General") => k.to_string(),
                    Some(s) => format!("{s}/{k}"),
                };
                raw.insert(full, v.to_string());
            }
        }
        settings.raw = raw.clone();

        let g = |key: &str| raw.get(key).cloned().or_else(|| get(key));

        if let Some(v) = g("core/coreMode") {
            settings.core_mode = CoreMode::from_ini(&v);
        }
        if let Some(v) = g("core/processMode") {
            settings.process_mode = ProcessMode::from_ini(&v);
        }
        if let Some(v) = g("client/remoteHost") {
            settings.remote_host = v;
        }
        if let Some(v) = g("core/computerName").or_else(|| g("core/screenName")) {
            settings.computer_name = v;
        }
        if let Some(v) = g("core/port") {
            if let Ok(p) = v.parse() {
                settings.port = p;
            }
        }
        if let Some(v) = g("core/interface") {
            settings.interface = v;
        }
        if let Some(v) = g("log/level") {
            settings.log_level = v;
        }
        if let Some(v) = g("gui/closeToTray") {
            settings.close_to_tray = parse_bool(&v, true);
        }
        if let Some(v) = g("gui/startCoreWithGui") {
            settings.auto_start_core = parse_bool(&v, false);
        }
        if let Some(v) = g("security/tlsEnabled") {
            settings.tls_enabled = parse_bool(&v, true);
        }
        if let Some(v) = g("security/checkPeerFingerprints") {
            settings.check_peers = parse_bool(&v, true);
        }
        if let Some(v) = g("daemon/elevate") {
            settings.elevate = parse_bool(&v, cfg!(windows));
        }
        if let Some(v) = g("core/preventSleep") {
            settings.prevent_sleep = parse_bool(&v, false);
        }
        if let Some(v) = g("client/languageSync") {
            settings.language_sync = parse_bool(&v, true);
        }
        if let Some(v) = g("client/invertYScroll") {
            settings.invert_y_scroll = parse_bool(&v, false);
        }
        if let Some(v) = g("client/invertXScroll") {
            settings.invert_x_scroll = parse_bool(&v, false);
        }
        if let Some(v) = g("client/yScrollScale") {
            settings.y_scroll_scale = v.parse().unwrap_or(1.0);
        }
        if let Some(v) = g("client/xScrollScale") {
            settings.x_scroll_scale = v.parse().unwrap_or(1.0);
        }
        if let Some(v) = g("client/dynamicConnectionInterval") {
            settings.dynamic_connection_retry = parse_bool(&v, true);
        }
        if let Some(v) = g("server/externalConfig") {
            settings.external_config = parse_bool(&v, false);
        }
        if let Some(v) = g("server/externalConfigFile") {
            settings.external_config_file = v;
        } else {
            settings.external_config_file = self
                .settings_path()
                .join(format!("{}-server.conf", version::info().app_id))
                .to_string_lossy()
                .into();
        }
        if let Some(v) = g("server/protocol") {
            settings.protocol = v;
        }

        settings
    }

    pub fn save(&self, settings: &AppSettings) -> Result<(), String> {
        if let Some(parent) = self.path.parent() {
            fs::create_dir_all(parent).map_err(|e| e.to_string())?;
        }

        let mut ini = if self.path.exists() {
            Ini::load_from_file(&self.path).unwrap_or_default()
        } else {
            Ini::new()
        };

        let set = |ini: &mut Ini, key: &str, value: String| {
            // Store as Qt-style path keys in General section
            ini.set_to(Some("General"), key.to_string(), value);
        };

        set(&mut ini, "core/coreMode", settings.core_mode.as_ini().into());
        set(
            &mut ini,
            "core/processMode",
            settings.process_mode.as_ini().into(),
        );
        set(&mut ini, "client/remoteHost", settings.remote_host.clone());
        set(&mut ini, "core/computerName", settings.computer_name.clone());
        set(&mut ini, "core/port", settings.port.to_string());
        set(&mut ini, "core/interface", settings.interface.clone());
        set(&mut ini, "log/level", settings.log_level.clone());
        set(
            &mut ini,
            "gui/closeToTray",
            bool_str(settings.close_to_tray),
        );
        set(
            &mut ini,
            "gui/startCoreWithGui",
            bool_str(settings.auto_start_core),
        );
        set(&mut ini, "security/tlsEnabled", bool_str(settings.tls_enabled));
        set(
            &mut ini,
            "security/checkPeerFingerprints",
            bool_str(settings.check_peers),
        );
        set(&mut ini, "daemon/elevate", bool_str(settings.elevate));
        set(&mut ini, "core/preventSleep", bool_str(settings.prevent_sleep));
        set(&mut ini, "client/languageSync", bool_str(settings.language_sync));
        set(
            &mut ini,
            "client/invertYScroll",
            bool_str(settings.invert_y_scroll),
        );
        set(
            &mut ini,
            "client/invertXScroll",
            bool_str(settings.invert_x_scroll),
        );
        set(
            &mut ini,
            "client/yScrollScale",
            settings.y_scroll_scale.to_string(),
        );
        set(
            &mut ini,
            "client/xScrollScale",
            settings.x_scroll_scale.to_string(),
        );
        set(
            &mut ini,
            "client/dynamicConnectionInterval",
            bool_str(settings.dynamic_connection_retry),
        );
        set(
            &mut ini,
            "server/externalConfig",
            bool_str(settings.external_config),
        );
        set(
            &mut ini,
            "server/externalConfigFile",
            settings.external_config_file.clone(),
        );
        set(&mut ini, "server/protocol", settings.protocol.clone());
        set(
            &mut ini,
            "core/lastVersion",
            version::info().version.clone(),
        );

        ini.write_to_file(&self.path).map_err(|e| e.to_string())
    }

    pub fn trust_fingerprint(&self, fingerprint: &str, as_server: bool) -> Result<(), String> {
        let path = if as_server {
            self.tls_trusted_servers_db()
        } else {
            self.tls_trusted_clients_db()
        };
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent).map_err(|e| e.to_string())?;
        }
        let mut existing = if path.exists() {
            fs::read_to_string(&path).unwrap_or_default()
        } else {
            String::new()
        };
        if !existing.contains(fingerprint) {
            if !existing.is_empty() && !existing.ends_with('\n') {
                existing.push('\n');
            }
            existing.push_str(fingerprint);
            existing.push('\n');
            fs::write(&path, existing).map_err(|e| e.to_string())?;
        }
        Ok(())
    }

    pub fn path(&self) -> &Path {
        &self.path
    }
}

fn parse_bool(value: &str, default: bool) -> bool {
    match value.trim().to_ascii_lowercase().as_str() {
        "1" | "true" | "yes" | "on" => true,
        "0" | "false" | "no" | "off" => false,
        _ => default,
    }
}

fn bool_str(v: bool) -> String {
    if v { "true" } else { "false" }.into()
}
