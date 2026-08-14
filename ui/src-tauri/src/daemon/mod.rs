use crate::ipc::{IpcClient, IpcError, IpcEvent};
use crate::version;
use serde::{Deserialize, Serialize};
use thiserror::Error;
use tokio::sync::mpsc;
use tokio::time::{timeout, Duration};

#[derive(Debug, Error)]
pub enum DaemonError {
    #[error(transparent)]
    Ipc(#[from] IpcError),
    #[error("{0}")]
    Other(String),
}

pub struct DaemonClient {
    ipc: IpcClient,
    events: mpsc::UnboundedReceiver<IpcEvent>,
}

#[derive(Debug, Clone, Default, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct DaemonStatus {
    pub state: String,
    pub process_id: u32,
    pub session_id: u32,
    pub integrity_rid: u32,
    pub elevated: bool,
    pub ui_access: bool,
    pub start_failures: i32,
    pub last_error: String,
    pub config_file: String,
}

impl DaemonClient {
    pub async fn connect() -> Result<Self, DaemonError> {
        let (tx, rx) = IpcClient::event_channel();
        let name = version::info().daemon_ipc_name.clone();
        let mut ipc = IpcClient::connect(&name, tx).await?;
        if let Some(server) = ipc.mismatched_server_version().map(str::to_string) {
            // Stale daemon cannot be "fixed" by stop — binaries must be reinstalled together.
            let _ = ipc.send_stop().await;
            ipc.disconnect();
            return Err(DaemonError::Other(format!(
                "daemon version mismatch (gui={}, daemon={}). Reinstall Deskflow so GUI, daemon, and core match.",
                version::info().version_id,
                server
            )));
        }
        Ok(Self { ipc, events: rx })
    }

    pub async fn send_log_level(&mut self, level: &str) -> Result<(), DaemonError> {
        self.send_expect_ok(&format!("logLevel={level}"), "logLevel")
            .await
    }

    pub async fn send_config_file(&mut self, path: &str) -> Result<(), DaemonError> {
        self.send_expect_ok(&format!("configFile={path}"), "configFile")
            .await
    }

    pub async fn send_start(&mut self) -> Result<(), DaemonError> {
        self.send_expect_ok("start", "start").await
    }

    pub async fn send_stop(&mut self) -> Result<(), DaemonError> {
        self.send_expect_ok("stop", "stop").await
    }

    pub async fn send_clear_settings(&mut self) -> Result<(), DaemonError> {
        self.send_expect_ok("clearSettings", "clearSettings").await
    }

    pub async fn request_log_path(&mut self) -> Result<(), DaemonError> {
        self.ipc.send("logPath").await.map_err(Into::into)
    }

    pub async fn status(&mut self) -> Result<DaemonStatus, DaemonError> {
        self.ipc.send("status").await?;
        let value = self.wait_for_message("status").await?;
        serde_json::from_str(&value)
            .map_err(|e| DaemonError::Other(format!("invalid daemon status: {e}")))
    }

    async fn send_expect_ok(&mut self, message: &str, command: &str) -> Result<(), DaemonError> {
        self.ipc.send(message).await?;
        let ack = self.wait_for_message("ok").await?;
        if ack == command {
            Ok(())
        } else {
            Err(DaemonError::Other(format!(
                "unexpected daemon acknowledgement for {command}: {ack}"
            )))
        }
    }

    async fn wait_for_message(&mut self, expected: &str) -> Result<String, DaemonError> {
        timeout(Duration::from_secs(3), async {
            while let Some(event) = self.events.recv().await {
                match event {
                    IpcEvent::Connected => continue,
                    IpcEvent::Message(message) if message.command == expected => {
                        return Ok(message.args);
                    }
                    IpcEvent::Message(message) if message.command == "error" => {
                        return Err(DaemonError::Other(message.args));
                    }
                    IpcEvent::ConnectionFailed { reason } => {
                        return Err(DaemonError::Other(format!(
                            "daemon connection failed: {reason}"
                        )));
                    }
                    IpcEvent::ServerShutdown => {
                        return Err(DaemonError::Other("daemon shut down".into()));
                    }
                    IpcEvent::VersionMismatch { server_version } => {
                        return Err(DaemonError::Other(format!(
                            "daemon version mismatch: {server_version}"
                        )));
                    }
                    _ => continue,
                }
            }
            Err(DaemonError::Other("daemon connection closed".into()))
        })
        .await
        .map_err(|_| DaemonError::Other(format!("daemon {expected} response timed out")))?
    }
}
