use crate::config::{AppSettings, CoreMode, ProcessMode, ServerConfig, SettingsStore};
use crate::daemon::DaemonClient;
use crate::ipc::{IpcClient, IpcEvent};
use crate::version;
use serde::{Deserialize, Serialize};
use std::path::PathBuf;
use std::process::Stdio;
use std::sync::Arc;
use tauri::{AppHandle, Emitter};
use tokio::io::{AsyncBufReadExt, BufReader};
use tokio::process::{Child, Command};
use tokio::sync::Mutex;
use tokio::time::{sleep, Duration};

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "camelCase")]
pub enum ProcessState {
    Stopped,
    Starting,
    Started,
    Stopping,
    RetryPending,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "camelCase")]
pub enum ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Listening,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct CoreStatus {
    pub process_state: ProcessState,
    pub connection_state: ConnectionState,
    pub connected_clients: Vec<String>,
    pub secure_socket: Option<String>,
    pub mode: CoreMode,
    pub process_mode: ProcessMode,
    pub version_id: String,
    pub local_ips: Vec<String>,
    pub computer_name: String,
    pub peer_fingerprint: Option<String>,
    pub retry_in: Option<i32>,
    pub last_error: Option<String>,
}

impl Default for CoreStatus {
    fn default() -> Self {
        Self {
            process_state: ProcessState::Stopped,
            connection_state: ConnectionState::Disconnected,
            connected_clients: vec![],
            secure_socket: None,
            mode: CoreMode::None,
            process_mode: ProcessMode::Desktop,
            version_id: version::info().version_id.clone(),
            local_ips: local_ips(),
            computer_name: hostname::get()
                .ok()
                .and_then(|h| h.into_string().ok())
                .unwrap_or_default(),
            peer_fingerprint: None,
            retry_in: None,
            last_error: None,
        }
    }
}

pub struct CoreProcess {
    pub status: CoreStatus,
    child: Option<Child>,
    ipc: Option<IpcClient>,
    settings_store: SettingsStore,
    settings: AppSettings,
    server_config: ServerConfig,
    app: Option<AppHandle>,
}

impl CoreProcess {
    pub fn new() -> Self {
        let settings_store = SettingsStore::new();
        let settings = settings_store.load();
        let server_config = {
            let path = settings_store.server_config_file(&settings);
            let mut config = ServerConfig::load_conf(&path).unwrap_or_default();
            config.mark_server_by_name(&settings.computer_name);
            config
        };
        let mut status = CoreStatus::default();
        status.mode = settings.core_mode;
        status.process_mode = settings.process_mode;
        status.computer_name = settings.computer_name.clone();
        Self {
            status,
            child: None,
            ipc: None,
            settings_store,
            settings,
            server_config,
            app: None,
        }
    }

    pub fn set_app(&mut self, app: AppHandle) {
        self.app = Some(app);
    }

    pub fn settings(&self) -> &AppSettings {
        &self.settings
    }

    pub fn settings_mut(&mut self) -> &mut AppSettings {
        &mut self.settings
    }

    pub fn server_config(&self) -> &ServerConfig {
        &self.server_config
    }

    pub fn server_config_mut(&mut self) -> &mut ServerConfig {
        &mut self.server_config
    }

    pub fn settings_store(&self) -> &SettingsStore {
        &self.settings_store
    }

    pub fn save_settings(&mut self) -> Result<(), String> {
        self.settings_store.save(&self.settings)?;
        self.status.mode = self.settings.core_mode;
        self.status.process_mode = self.settings.process_mode;
        self.status.computer_name = self.settings.computer_name.clone();
        Ok(())
    }

    pub fn persist_server_config(&self) -> Result<PathBuf, String> {
        let path = self.settings_store.server_config_file(&self.settings);
        self.server_config.save_conf(&path)?;
        Ok(path)
    }

    fn emit_log(&self, line: &str) {
        if let Some(app) = &self.app {
            let _ = app.emit("deskflow-log", line.to_string());
        }
    }

    fn emit_status(&self) {
        if let Some(app) = &self.app {
            let _ = app.emit("deskflow-process", &self.status);
        }
    }

    fn emit_ipc(&self, event: &IpcEvent) {
        if let Some(app) = &self.app {
            let _ = app.emit("deskflow-ipc", event);
        }
    }

    fn resolve_core_binary(&self) -> Result<PathBuf, String> {
        let bin = if cfg!(windows) {
            format!("{}.exe", version::info().core_bin_name)
        } else {
            version::info().core_bin_name.clone()
        };

        let mut candidates = Vec::new();
        if let Ok(exe) = std::env::current_exe() {
            if let Some(dir) = exe.parent() {
                candidates.push(dir.join(&bin));
            }
        }
        // Dev fallback: repo build/bin
        if let Ok(cwd) = std::env::current_dir() {
            candidates.push(cwd.join("build").join("bin").join(&bin));
            candidates.push(cwd.join("..").join("build").join("bin").join(&bin));
            candidates.push(cwd.join("..").join("..").join("build").join("bin").join(&bin));
        }
        if let Ok(override_path) = std::env::var("DESKFLOW_CORE_PATH") {
            candidates.insert(0, PathBuf::from(override_path));
        }

        for path in candidates {
            if path.exists() {
                return Ok(path);
            }
        }
        Err(format!(
            "core binary '{bin}' not found beside app or in build/bin (set DESKFLOW_CORE_PATH)"
        ))
    }

    pub async fn start(this: Arc<Mutex<Self>>) -> Result<(), String> {
        {
            let mut guard = this.lock().await;
            if matches!(
                guard.status.process_state,
                ProcessState::Starting | ProcessState::Started
            ) {
                return Err("core already running".into());
            }
            guard.status.process_state = ProcessState::Starting;
            guard.status.connection_state = ConnectionState::Connecting;
            guard.status.last_error = None;
            guard.status.connected_clients.clear();
            guard.status.peer_fingerprint = None;
            guard.emit_status();
            guard.emit_log("starting core...");
        }

        let (process_mode, mode) = {
            let guard = this.lock().await;
            (guard.settings.process_mode, guard.settings.core_mode)
        };

        if mode == CoreMode::None {
            let mut guard = this.lock().await;
            guard.status.process_state = ProcessState::Stopped;
            guard.status.connection_state = ConnectionState::Disconnected;
            guard.status.last_error = Some("select server or client mode".into());
            guard.emit_status();
            return Err("select server or client mode".into());
        }

        let result = match process_mode {
            ProcessMode::Desktop => Self::start_desktop(this.clone(), mode).await,
            ProcessMode::Service => Self::start_service(this.clone(), mode).await,
        };

        if let Err(e) = &result {
            let mut guard = this.lock().await;
            guard.status.process_state = ProcessState::Stopped;
            guard.status.connection_state = ConnectionState::Disconnected;
            guard.status.last_error = Some(e.clone());
            guard.emit_log(&format!("start failed: {e}"));
            guard.emit_status();
        }
        result
    }

    async fn start_desktop(this: Arc<Mutex<Self>>, mode: CoreMode) -> Result<(), String> {
        let (core_path, args) = {
            let guard = this.lock().await;
            if mode == CoreMode::Server {
                let path = guard.persist_server_config()?;
                guard.emit_log(&format!("wrote server config: {}", path.display()));
            }
            let core_path = guard.resolve_core_binary()?;
            let mut args = vec![match mode {
                CoreMode::Server => "server".to_string(),
                CoreMode::Client => "client".to_string(),
                CoreMode::None => unreachable!(),
            }];
            // Pass settings path so client/server pick up the same INI
            let settings_path = guard.settings_store.path().to_path_buf();
            if settings_path.exists() {
                args.push("--settings".into());
                args.push(settings_path.to_string_lossy().into());
            }
            (core_path, args)
        };
        let mut cmd = Command::new(&core_path);
        cmd.args(&args)
            .stdout(Stdio::piped())
            .stderr(Stdio::piped())
            .kill_on_drop(true);

        let mut child = cmd.spawn().map_err(|e| format!("spawn failed: {e}"))?;

        if let Some(stdout) = child.stdout.take() {
            let app_this = this.clone();
            tokio::spawn(async move {
                let mut lines = BufReader::new(stdout).lines();
                while let Ok(Some(line)) = lines.next_line().await {
                    let guard = app_this.lock().await;
                    guard.emit_log(&line);
                }
            });
        }
        if let Some(stderr) = child.stderr.take() {
            let app_this = this.clone();
            tokio::spawn(async move {
                let mut lines = BufReader::new(stderr).lines();
                while let Ok(Some(line)) = lines.next_line().await {
                    let guard = app_this.lock().await;
                    guard.emit_log(&line);
                }
            });
        }

        {
            let mut guard = this.lock().await;
            guard.child = Some(child);
            guard.status.process_state = ProcessState::Started;
            guard.emit_status();
            guard.emit_log(&format!("spawned {} {}", core_path.display(), args.join(" ")));
        }

        // Match Qt: wait briefly then connect IPC
        sleep(Duration::from_secs(1)).await;
        Self::connect_core_ipc(this.clone()).await?;

        // Watch child exit
        let watch = this.clone();
        tokio::spawn(async move {
            loop {
                sleep(Duration::from_millis(500)).await;
                let mut guard = watch.lock().await;
                if let Some(child) = guard.child.as_mut() {
                    match child.try_wait() {
                        Ok(Some(status)) => {
                            guard.emit_log(&format!("core exited: {status}"));
                            guard.child = None;
                            guard.ipc = None;
                            guard.status.process_state = ProcessState::Stopped;
                            guard.status.connection_state = ConnectionState::Disconnected;
                            guard.emit_status();
                            break;
                        }
                        Ok(None) => {}
                        Err(e) => {
                            guard.emit_log(&format!("wait error: {e}"));
                            break;
                        }
                    }
                } else {
                    break;
                }
            }
        });

        Ok(())
    }

    async fn start_service(this: Arc<Mutex<Self>>, mode: CoreMode) -> Result<(), String> {
        let (conf_path, log_level) = {
            let guard = this.lock().await;
            if mode == CoreMode::Server {
                let path = guard.persist_server_config()?;
                guard.emit_log(&format!("wrote server config: {}", path.display()));
            }
            let conf_hint = guard.settings_store.server_config_file(&guard.settings);
            (conf_hint, guard.settings.log_level.clone())
        };

        let settings_file = {
            let guard = this.lock().await;
            guard.settings_store.path().to_path_buf()
        };

        let mut daemon = DaemonClient::connect().await.map_err(|e| e.to_string())?;
        daemon
            .send_log_level(&log_level)
            .await
            .map_err(|e| e.to_string())?;
        daemon
            .send_config_file(&settings_file.to_string_lossy())
            .await
            .map_err(|e| e.to_string())?;
        daemon.send_start().await.map_err(|e| e.to_string())?;

        {
            let mut guard = this.lock().await;
            guard.status.process_state = ProcessState::Started;
            guard.emit_log(&format!(
                "daemon start issued (settings={}, conf hint={})",
                settings_file.display(),
                conf_path.display()
            ));
            guard.emit_status();
        }

        sleep(Duration::from_secs(1)).await;
        let _ = Self::connect_core_ipc(this.clone()).await;

        // Tail daemon log
        let log_path = {
            let guard = this.lock().await;
            guard.settings_store.daemon_log_file()
        };
        let tail_this = this.clone();
        tokio::spawn(async move {
            let mut pos = 0u64;
            if let Ok(meta) = tokio::fs::metadata(&log_path).await {
                pos = meta.len().saturating_sub(4096);
            }
            loop {
                sleep(Duration::from_millis(800)).await;
                if let Ok(mut file) = tokio::fs::File::open(&log_path).await {
                    use tokio::io::{AsyncReadExt, AsyncSeekExt, SeekFrom};
                    if file.seek(SeekFrom::Start(pos)).await.is_ok() {
                        let mut buf = String::new();
                        if file.read_to_string(&mut buf).await.is_ok() && !buf.is_empty() {
                            pos += buf.len() as u64;
                            let guard = tail_this.lock().await;
                            for line in buf.lines() {
                                guard.emit_log(line);
                            }
                            if matches!(guard.status.process_state, ProcessState::Stopped | ProcessState::Stopping)
                            {
                                break;
                            }
                        }
                    }
                }
                let guard = tail_this.lock().await;
                if matches!(guard.status.process_state, ProcessState::Stopped | ProcessState::Stopping)
                {
                    break;
                }
            }
        });

        Ok(())
    }

    async fn connect_core_ipc(this: Arc<Mutex<Self>>) -> Result<(), String> {
        let (event_tx, mut event_rx) = IpcClient::event_channel();
        let socket = version::info().core_ipc_name.clone();
        let client = IpcClient::connect(&socket, event_tx)
            .await
            .map_err(|e| e.to_string())?;

        {
            let mut guard = this.lock().await;
            guard.ipc = Some(client);
            guard.emit_log("core ipc connected");
        }

        let fwd = this.clone();
        tokio::spawn(async move {
            while let Some(event) = event_rx.recv().await {
                let mut guard = fwd.lock().await;
                guard.emit_ipc(&event);
                match &event {
                    IpcEvent::Message(msg) => match msg.command.as_str() {
                        "connectionState" => {
                            guard.status.connection_state = match msg.args.as_str() {
                                "Connected" => ConnectionState::Connected,
                                "Connecting" => ConnectionState::Connecting,
                                "Listening" => ConnectionState::Listening,
                                _ => ConnectionState::Disconnected,
                            };
                            guard.emit_status();
                        }
                        "connectedClients" => {
                            guard.status.connected_clients = if msg.args.is_empty() {
                                vec![]
                            } else {
                                msg.args.split(',').map(|s| s.trim().to_string()).filter(|s| !s.is_empty()).collect()
                            };
                            guard.emit_status();
                        }
                        "secureSocket" => {
                            guard.status.secure_socket = Some(msg.args.clone());
                            guard.emit_status();
                        }
                        "peerFingerprint" => {
                            guard.status.peer_fingerprint = Some(msg.args.clone());
                            guard.emit_status();
                        }
                        "retryIn" => {
                            guard.status.retry_in = msg.args.parse().ok();
                            guard.emit_status();
                        }
                        "unrecognisedClient" => {
                            guard.emit_log(&format!("unrecognised client: {}", msg.args));
                        }
                        "connectionRefused" => {
                            guard.status.last_error = Some(format!("connection refused: {}", msg.args));
                            guard.emit_status();
                        }
                        "missingKeyboardLayouts" => {
                            guard.emit_log(&format!("missing keyboard layouts: {}", msg.args));
                        }
                        _ => {}
                    },
                    IpcEvent::ServerShutdown | IpcEvent::ConnectionFailed { .. } => {
                        if !matches!(guard.status.process_state, ProcessState::Stopping) {
                            // leave process state; child watcher handles stop
                        }
                        guard.status.connection_state = ConnectionState::Disconnected;
                        guard.emit_status();
                    }
                    IpcEvent::VersionMismatch { server_version } => {
                        guard.status.last_error =
                            Some(format!("version mismatch with core: {server_version}"));
                        guard.emit_status();
                    }
                    IpcEvent::Connected => {}
                }
            }
        });

        Ok(())
    }

    pub async fn stop(this: Arc<Mutex<Self>>) -> Result<(), String> {
        {
            let mut guard = this.lock().await;
            guard.status.process_state = ProcessState::Stopping;
            guard.emit_status();
            guard.emit_log("stopping core...");
        }

        let process_mode = {
            let guard = this.lock().await;
            guard.settings.process_mode
        };

        if process_mode == ProcessMode::Service {
            if let Ok(mut daemon) = DaemonClient::connect().await {
                let _ = daemon.send_stop().await;
            }
        }

        {
            let mut guard = this.lock().await;
            if let Some(ipc) = guard.ipc.as_mut() {
                let _ = ipc.send_stop().await;
            }
        }

        sleep(Duration::from_millis(500)).await;

        {
            let mut guard = this.lock().await;
            if let Some(mut child) = guard.child.take() {
                let _ = child.kill().await;
            }
            guard.ipc = None;
            guard.status.process_state = ProcessState::Stopped;
            guard.status.connection_state = ConnectionState::Disconnected;
            guard.status.connected_clients.clear();
            guard.emit_log("core stopped");
            guard.emit_status();
        }
        Ok(())
    }

    pub async fn restart(this: Arc<Mutex<Self>>) -> Result<(), String> {
        Self::stop(this.clone()).await?;
        sleep(Duration::from_millis(400)).await;
        Self::start(this).await
    }

    /// If core is already running (service watchdog or prior session), attach IPC
    /// so the GUI reflects that process instead of looking disconnected.
    pub async fn attach_if_running(this: Arc<Mutex<Self>>) {
        {
            let guard = this.lock().await;
            if matches!(
                guard.status.process_state,
                ProcessState::Starting | ProcessState::Started
            ) {
                return;
            }
        }

        if Self::connect_core_ipc(this.clone()).await.is_err() {
            return;
        }

        let mut guard = this.lock().await;
        guard.status.process_mode = guard.settings.process_mode;
        guard.status.mode = guard.settings.core_mode;
        guard.status.process_state = ProcessState::Started;
        guard.emit_log("attached to already-running core");
        guard.emit_status();
    }
}

pub fn local_ips_public() -> Vec<String> {
    local_ips()
}

fn local_ips() -> Vec<String> {
    let mut ips = Vec::new();
    if let Ok(ip) = local_ip_address::local_ip() {
        ips.push(ip.to_string());
    }
    if let Ok(list) = local_ip_address::list_afinet_netifas() {
        for (_name, ip) in list {
            let s = ip.to_string();
            if !ips.contains(&s) && ip.is_ipv4() && !ip.is_loopback() {
                ips.push(s);
            }
        }
    }
    ips
}
