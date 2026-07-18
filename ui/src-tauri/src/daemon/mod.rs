use crate::ipc::{IpcClient, IpcError, IpcEvent};
use crate::version;
use thiserror::Error;
use tokio::sync::mpsc;

#[derive(Debug, Error)]
pub enum DaemonError {
    #[error(transparent)]
    Ipc(#[from] IpcError),
    #[error("{0}")]
    Other(String),
}

pub struct DaemonClient {
    ipc: IpcClient,
    _events: mpsc::UnboundedReceiver<IpcEvent>,
}

impl DaemonClient {
    pub async fn connect() -> Result<Self, DaemonError> {
        let (tx, rx) = IpcClient::event_channel();
        let name = version::info().daemon_ipc_name.clone();
        let ipc = IpcClient::connect(&name, tx).await?;
        Ok(Self {
            ipc,
            _events: rx,
        })
    }

    pub async fn send_log_level(&mut self, level: &str) -> Result<(), DaemonError> {
        self.ipc
            .send(&format!("logLevel={level}"))
            .await
            .map_err(Into::into)
    }

    pub async fn send_config_file(&mut self, path: &str) -> Result<(), DaemonError> {
        self.ipc
            .send(&format!("configFile={path}"))
            .await
            .map_err(Into::into)
    }

    pub async fn send_start(&mut self) -> Result<(), DaemonError> {
        self.ipc.send("start").await.map_err(Into::into)
    }

    pub async fn send_stop(&mut self) -> Result<(), DaemonError> {
        self.ipc.send("stop").await.map_err(Into::into)
    }

    pub async fn send_clear_settings(&mut self) -> Result<(), DaemonError> {
        self.ipc.send("clearSettings").await.map_err(Into::into)
    }

    pub async fn request_log_path(&mut self) -> Result<(), DaemonError> {
        self.ipc.send("logPath").await.map_err(Into::into)
    }
}
