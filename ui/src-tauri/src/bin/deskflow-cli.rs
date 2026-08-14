//! deskflow-cli — version-locked Core IPC helper for pause/resume/toggle/status.
//!
//! Exit codes:
//!   0 — paused
//!   1 — active (not paused)
//!   2+ — unreachable, version mismatch, or other error

use deskflow_ui_lib::ipc::{IpcClient, IpcEvent};
use deskflow_ui_lib::version;
use std::env;
use std::process::ExitCode;
use std::time::Duration;
use tokio::sync::mpsc;
use tokio::time::timeout;

const USAGE: &str = "\
Usage: deskflow-cli <pause|resume|toggle|status>

Exit codes: 0=paused, 1=active, 2+=error
";

#[tokio::main(flavor = "current_thread")]
async fn main() -> ExitCode {
    let mut args = env::args().skip(1);
    let Some(cmd) = args.next() else {
        eprint!("{USAGE}");
        return ExitCode::from(2);
    };
    if args.next().is_some() {
        eprint!("{USAGE}");
        return ExitCode::from(2);
    }

    let command = match cmd.as_str() {
        "pause" | "resume" | "toggle" | "status" => cmd,
        "-h" | "--help" | "help" => {
            print!("{USAGE}");
            return ExitCode::SUCCESS;
        }
        other => {
            eprintln!("unknown command: {other}");
            eprint!("{USAGE}");
            return ExitCode::from(2);
        }
    };

    match run_pause_command(&command).await {
        Ok(paused) => {
            println!("paused={}", if paused { "true" } else { "false" });
            if paused {
                ExitCode::from(0)
            } else {
                ExitCode::from(1)
            }
        }
        Err(err) => {
            eprintln!("{err}");
            ExitCode::from(2)
        }
    }
}

async fn run_pause_command(command: &str) -> Result<bool, String> {
    let socket = version::info().core_ipc_name.clone();
    let (event_tx, mut event_rx) = IpcClient::event_channel();

    let mut client = IpcClient::connect(&socket, event_tx)
        .await
        .map_err(|e| format!("connect failed: {e}"))?;

    if let Some(server_version) = client.mismatched_server_version().map(str::to_owned) {
        let _ = client.send_stop().await;
        return Err(format!(
            "version mismatch (cli={}, server={server_version})",
            version::info().version_id
        ));
    }

    // Drop the Connected event (and any replayed state) so we only wait on our reply.
    drain_briefly(&mut event_rx, Duration::from_millis(50)).await;

    client
        .send(command)
        .await
        .map_err(|e| format!("send failed: {e}"))?;

    wait_for_paused(command, &mut event_rx, Duration::from_secs(3)).await
}

async fn drain_briefly(event_rx: &mut mpsc::UnboundedReceiver<IpcEvent>, wait: Duration) {
    let deadline = tokio::time::Instant::now() + wait;
    loop {
        let remaining = deadline.saturating_duration_since(tokio::time::Instant::now());
        if remaining.is_zero() {
            break;
        }
        match timeout(remaining, event_rx.recv()).await {
            Ok(Some(_)) => continue,
            Ok(None) | Err(_) => break,
        }
    }
}

async fn wait_for_paused(
    command: &str,
    event_rx: &mut mpsc::UnboundedReceiver<IpcEvent>,
    wait: Duration,
) -> Result<bool, String> {
    let deadline = tokio::time::Instant::now() + wait;
    let mut saw_ok = false;

    loop {
        let remaining = deadline.saturating_duration_since(tokio::time::Instant::now());
        if remaining.is_zero() {
            return Err(if saw_ok {
                "timed out waiting for paused= reply".into()
            } else {
                format!("timed out waiting for ok={command}")
            });
        }

        match timeout(remaining, event_rx.recv()).await {
            Err(_) => {
                return Err(if saw_ok {
                    "timed out waiting for paused= reply".into()
                } else {
                    format!("timed out waiting for ok={command}")
                });
            }
            Ok(None) => return Err("ipc connection closed".into()),
            Ok(Some(IpcEvent::Connected)) => {}
            Ok(Some(IpcEvent::ServerShutdown)) => return Err("core shut down".into()),
            Ok(Some(IpcEvent::ConnectionFailed { reason })) => {
                return Err(format!("ipc connection failed: {reason}"));
            }
            Ok(Some(IpcEvent::VersionMismatch { server_version })) => {
                return Err(format!(
                    "version mismatch (cli={}, server={server_version})",
                    version::info().version_id
                ));
            }
            Ok(Some(IpcEvent::Message(msg))) => {
                if msg.command == "ok" && msg.args == command {
                    saw_ok = true;
                    continue;
                }
                if msg.command == "error" {
                    return Err(if msg.args.is_empty() {
                        "core returned error".into()
                    } else {
                        format!("core error: {}", msg.args)
                    });
                }
                if saw_ok && msg.command == "paused" {
                    return match msg.args.as_str() {
                        "true" => Ok(true),
                        "false" => Ok(false),
                        other => Err(format!("unexpected paused value: {other}")),
                    };
                }
            }
        }
    }
}
