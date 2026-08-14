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
    source_path: PathBuf,
}

impl SettingsStore {
    pub fn new() -> Self {
        let path = Self::settings_file_path();
        let source_path = Self::settings_source_file(&path);
        Self { path, source_path }
    }

    pub fn settings_file_path() -> PathBuf {
        if let Some(portable) = Self::portable_settings_file() {
            if portable.exists() {
                return portable;
            }
        }
        Self::user_settings_file()
    }

    fn settings_source_file(canonical: &Path) -> PathBuf {
        if canonical.exists() {
            return canonical.to_path_buf();
        }

        #[cfg(windows)]
        {
            let system = Self::system_settings_file();
            if system.exists() {
                return system;
            }
        }

        canonical.to_path_buf()
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

    pub fn system_settings_file() -> PathBuf {
        Self::system_dir().join(format!("{}.conf", version::info().app_name))
    }

    pub fn portable_settings_file() -> Option<PathBuf> {
        std::env::current_exe().ok().and_then(|exe| {
            exe.parent().map(|p| {
                p.join("settings")
                    .join(format!("{}.conf", version::info().app_name))
            })
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
        settings.process_mode = self.environment_process_mode();

        if !self.source_path.exists() {
            return settings;
        }
        let Ok(ini) = Ini::load_from_file(&self.source_path) else {
            return settings;
        };

        // Keep a flattened view for the UI, but read known settings through
        // `get_qsettings_value` below. Earlier Tauri builds wrote literal
        // slash-containing keys in [General], which QSettings cannot read.
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

        let g = |key: &str| get_qsettings_value(&ini, key);

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
        } else if self.source_path.exists() {
            Ini::load_from_file(&self.source_path).unwrap_or_default()
        } else {
            Ini::new()
        };

        let set = |ini: &mut Ini, key: &str, value: String| {
            set_qsettings_value(ini, key, value);
        };

        set(
            &mut ini,
            "core/coreMode",
            settings.core_mode.as_ini().into(),
        );
        set(
            &mut ini,
            "core/processMode",
            settings.process_mode.as_ini().into(),
        );
        set(&mut ini, "client/remoteHost", settings.remote_host.clone());
        set(
            &mut ini,
            "core/computerName",
            settings.computer_name.clone(),
        );
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
        set(
            &mut ini,
            "security/tlsEnabled",
            bool_str(settings.tls_enabled),
        );
        set(
            &mut ini,
            "security/checkPeerFingerprints",
            bool_str(settings.check_peers),
        );
        set(&mut ini, "daemon/elevate", bool_str(settings.elevate));
        set(
            &mut ini,
            "core/preventSleep",
            bool_str(settings.prevent_sleep),
        );
        set(
            &mut ini,
            "client/languageSync",
            bool_str(settings.language_sync),
        );
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

    pub fn needs_normalization(&self) -> bool {
        if self.source_path != self.path {
            return true;
        }
        let Ok(ini) = Ini::load_from_file(&self.source_path) else {
            return false;
        };
        ini.section(Some("General"))
            .map(|section| section.iter().any(|(key, _)| key.contains('/')))
            .unwrap_or(false)
    }

    fn environment_process_mode(&self) -> ProcessMode {
        if Self::is_portable_mode() {
            return ProcessMode::Desktop;
        }

        #[cfg(windows)]
        {
            // Installed builds live under Program Files and have a Windows
            // service. Custom/remote installs write an explicit processMode,
            // while unpacked developer and portable builds remain Desktop.
            if executable_is_under_program_files() {
                return ProcessMode::Service;
            }
        }

        ProcessMode::Desktop
    }

    #[cfg(test)]
    fn for_test(path: PathBuf, source_path: PathBuf) -> Self {
        Self { path, source_path }
    }
}

fn get_qsettings_value(ini: &Ini, path: &str) -> Option<String> {
    // Prefer the malformed Tauri value when both forms exist: it represents
    // the user's most recent GUI save. The next save normalizes it.
    if let Some(value) = ini.get_from(Some("General"), path) {
        return Some(value.to_string());
    }
    if let Some(value) = ini.get_from(None::<String>, path) {
        return Some(value.to_string());
    }

    let (section, key) = path.split_once('/')?;
    ini.get_from(Some(section), key).map(ToString::to_string)
}

fn set_qsettings_value(ini: &mut Ini, path: &str, value: String) {
    // Remove the incompatible representation emitted by Tauri V2 before
    // writing the section/key form consumed by QSettings.
    ini.delete_from(Some("General"), path);
    ini.delete_from(None::<String>, path);

    if let Some((section, key)) = path.split_once('/') {
        ini.set_to(Some(section), key.to_string(), value);
    } else {
        ini.set_to(Some("General"), path.to_string(), value);
    }
}

#[cfg(windows)]
fn executable_is_under_program_files() -> bool {
    let Ok(exe) = std::env::current_exe() else {
        return false;
    };
    ["ProgramFiles", "ProgramFiles(x86)"]
        .into_iter()
        .filter_map(std::env::var_os)
        .map(PathBuf::from)
        .any(|root| exe.starts_with(root))
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

#[cfg(test)]
mod tests {
    use super::*;
    use std::time::{SystemTime, UNIX_EPOCH};

    fn temp_paths(name: &str) -> (PathBuf, PathBuf, PathBuf) {
        let nonce = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_nanos();
        let dir = std::env::temp_dir().join(format!("deskflow-settings-{name}-{nonce}"));
        let canonical = dir.join("user").join("Deskflow.conf");
        let source = dir.join("system").join("Deskflow.conf");
        (dir, canonical, source)
    }

    #[test]
    fn loads_legacy_qsettings_sections() {
        let (dir, canonical, source) = temp_paths("legacy");
        fs::create_dir_all(source.parent().unwrap()).unwrap();
        fs::write(
            &source,
            "[core]\ncoreMode=1\nprocessMode=0\n\n[client]\nremoteHost=10.0.0.4\n",
        )
        .unwrap();

        let settings = SettingsStore::for_test(canonical, source).load();
        assert_eq!(settings.core_mode, CoreMode::Client);
        assert_eq!(settings.process_mode, ProcessMode::Service);
        assert_eq!(settings.remote_host, "10.0.0.4");
        let _ = fs::remove_dir_all(dir);
    }

    #[test]
    fn malformed_tauri_values_win_once_and_are_normalized_on_save() {
        let (dir, canonical, source) = temp_paths("normalize");
        fs::create_dir_all(source.parent().unwrap()).unwrap();
        fs::write(
            &source,
            "[core]\ncoreMode=2\nprocessMode=1\n\n[General]\ncore/coreMode=1\ncore/processMode=0\n",
        )
        .unwrap();

        let store = SettingsStore::for_test(canonical.clone(), source);
        let settings = store.load();
        assert_eq!(settings.core_mode, CoreMode::Client);
        assert_eq!(settings.process_mode, ProcessMode::Service);
        store.save(&settings).unwrap();

        let written = fs::read_to_string(canonical).unwrap();
        assert!(written.contains("[core]"));
        assert!(written.contains("coreMode=1"));
        assert!(written.contains("processMode=0"));
        assert!(!written.contains("core/coreMode"));
        assert!(!written.contains("core/processMode"));
        let _ = fs::remove_dir_all(dir);
    }

    #[test]
    fn save_preserves_unknown_sections_from_fallback_source() {
        let (dir, canonical, source) = temp_paths("preserve");
        fs::create_dir_all(source.parent().unwrap()).unwrap();
        fs::write(&source, "[custom]\nkeepMe=yes\n\n[core]\ncoreMode=1\n").unwrap();

        let store = SettingsStore::for_test(canonical.clone(), source);
        let settings = store.load();
        store.save(&settings).unwrap();

        let written = fs::read_to_string(canonical).unwrap();
        assert!(written.contains("[custom]"));
        assert!(written.contains("keepMe=yes"));
        let _ = fs::remove_dir_all(dir);
    }
}
