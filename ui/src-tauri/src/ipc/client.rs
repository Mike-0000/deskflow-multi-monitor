use crate::version;
use interprocess::local_socket::tokio::{prelude::*, Stream};
use interprocess::local_socket::{GenericFilePath, GenericNamespaced, NameType, ToFsName, ToNsName};
use serde::Serialize;
use std::io;
use std::time::Duration;
use thiserror::Error;
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
use tokio::sync::mpsc;

#[derive(Debug, Error)]
pub enum IpcError {
    #[error("connect failed: {0}")]
    Connect(String),
    #[error("handshake failed: {0}")]
    Handshake(String),
    #[error("version mismatch: server={0}")]
    VersionMismatch(String),
    #[error("io error: {0}")]
    Io(#[from] io::Error),
    #[error("not connected")]
    NotConnected,
}

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct IpcMessage {
    pub command: String,
    pub args: String,
}

#[derive(Debug, Clone, Serialize)]
#[serde(tag = "type", rename_all = "camelCase")]
pub enum IpcEvent {
    Connected,
    ConnectionFailed { reason: String },
    VersionMismatch { server_version: String },
    ServerShutdown,
    Message(IpcMessage),
}

pub struct IpcClient {
    write: Option<tokio::io::WriteHalf<Stream>>,
    event_tx: mpsc::UnboundedSender<IpcEvent>,
}

impl IpcClient {
    pub fn event_channel() -> (mpsc::UnboundedSender<IpcEvent>, mpsc::UnboundedReceiver<IpcEvent>) {
        mpsc::unbounded_channel()
    }

    pub async fn connect(socket_name: &str, event_tx: mpsc::UnboundedSender<IpcEvent>) -> Result<Self, IpcError> {
        let mut last_err = String::from("unknown");
        for attempt in 1..=3 {
            match Self::try_connect_once(socket_name, event_tx.clone()).await {
                Ok(client) => return Ok(client),
                Err(e) => {
                    last_err = e.to_string();
                    log::warn!("ipc connect attempt {attempt} failed: {last_err}");
                    tokio::time::sleep(Duration::from_millis(200)).await;
                }
            }
        }
        let _ = event_tx.send(IpcEvent::ConnectionFailed {
            reason: last_err.clone(),
        });
        Err(IpcError::Connect(last_err))
    }

    async fn try_connect_once(
        socket_name: &str,
        event_tx: mpsc::UnboundedSender<IpcEvent>,
    ) -> Result<Self, IpcError> {
        let stream = connect_stream(socket_name).await?;
        let (read_half, mut write_half) = tokio::io::split(stream);

        let version_id = version::info().version_id.clone();
        let hello = format!("hello={version_id}\n");
        write_half.write_all(hello.as_bytes()).await?;
        write_half.flush().await?;

        let mut reader = BufReader::new(read_half);
        let mut line = String::new();
        let n = reader.read_line(&mut line).await?;
        if n == 0 {
            return Err(IpcError::Handshake("empty handshake".into()));
        }
        let line = line.trim_end_matches(['\r', '\n']).to_string();
        let (cmd, args) = split_message(&line);

        match cmd.as_str() {
            "error" => {
                return Err(IpcError::Handshake(args));
            }
            "versionMismatch" => {
                let _ = event_tx.send(IpcEvent::VersionMismatch {
                    server_version: args.clone(),
                });
                // Qt still marks Connected on mismatch; we do the same so stop can be sent.
            }
            "hello" => {
                if args.is_empty() {
                    return Err(IpcError::Handshake("missing version".into()));
                }
            }
            other => {
                return Err(IpcError::Handshake(format!("unexpected handshake: {other}")));
            }
        }

        let _ = event_tx.send(IpcEvent::Connected);

        let reader_tx = event_tx.clone();
        tokio::spawn(async move {
            let mut reader = reader;
            let mut buf = String::new();
            loop {
                buf.clear();
                match reader.read_line(&mut buf).await {
                    Ok(0) => {
                        let _ = reader_tx.send(IpcEvent::ConnectionFailed {
                            reason: "eof".into(),
                        });
                        break;
                    }
                    Ok(_) => {
                        let line = buf.trim_end_matches(['\r', '\n']).to_string();
                        if line.is_empty() {
                            continue;
                        }
                        let (command, args) = split_message(&line);
                        if command == "bye" {
                            let _ = reader_tx.send(IpcEvent::ServerShutdown);
                            break;
                        }
                        let _ = reader_tx.send(IpcEvent::Message(IpcMessage { command, args }));
                    }
                    Err(e) => {
                        let _ = reader_tx.send(IpcEvent::ConnectionFailed {
                            reason: e.to_string(),
                        });
                        break;
                    }
                }
            }
        });

        Ok(Self {
            write: Some(write_half),
            event_tx,
        })
    }

    pub async fn send(&mut self, message: &str) -> Result<(), IpcError> {
        let Some(write) = self.write.as_mut() else {
            return Err(IpcError::NotConnected);
        };
        let payload = format!("{message}\n");
        write.write_all(payload.as_bytes()).await?;
        write.flush().await?;
        Ok(())
    }

    pub async fn send_stop(&mut self) -> Result<(), IpcError> {
        self.send("stop").await
    }

    pub fn disconnect(&mut self) {
        self.write = None;
        let _ = self.event_tx.send(IpcEvent::ServerShutdown);
    }
}

fn split_message(line: &str) -> (String, String) {
    match line.split_once('=') {
        Some((cmd, args)) => (cmd.to_string(), args.to_string()),
        None => (line.to_string(), String::new()),
    }
}

async fn connect_stream(socket_name: &str) -> Result<Stream, IpcError> {
    // Prefer namespaced names (Windows named pipes / Linux abstract). Fall back to path.
    if GenericNamespaced::is_supported() {
        if let Ok(name) = socket_name.to_ns_name::<GenericNamespaced>() {
            match Stream::connect(name).await {
                Ok(s) => return Ok(s),
                Err(e) => log::debug!("namespaced connect failed: {e}"),
            }
        }
    }

    #[cfg(unix)]
    {
        let path = format!("/tmp/{socket_name}");
        if let Ok(name) = path.to_fs_name::<GenericFilePath>() {
            return Stream::connect(name)
                .await
                .map_err(|e| IpcError::Connect(e.to_string()));
        }
    }

    #[cfg(windows)]
    {
        // Qt QLocalSocket uses \\.\pipe\<name>
        let path = format!(r"\\.\pipe\{socket_name}");
        if let Ok(name) = path.to_fs_name::<GenericFilePath>() {
            return Stream::connect(name)
                .await
                .map_err(|e| IpcError::Connect(e.to_string()));
        }
    }

    Err(IpcError::Connect(format!(
        "unable to resolve local socket name '{socket_name}'"
    )))
}
