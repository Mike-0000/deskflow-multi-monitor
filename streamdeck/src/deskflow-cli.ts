import { access } from "node:fs/promises";
import { spawn, type ChildProcess } from "node:child_process";
import { constants as fsConstants } from "node:fs";

/** Default install path for the version-locked Deskflow CLI. */
export const DEFAULT_CLI_PATH = "C:\\Program Files\\Deskflow\\deskflow-cli.exe";

/** Kill hung CLI processes; must exceed deskflow-cli's internal ~3s reply wait. */
const CLI_TIMEOUT_MS = 5000;

export type DeskflowCliCommand = "pause" | "resume" | "toggle" | "status";

export type DeskflowCliResult = {
	/** Process exit code: 0=paused, 1=active, 2+=unreachable/mismatch/error. */
	exitCode: number;
	stdout: string;
	stderr: string;
	/** Parsed pause state when exit is 0 or 1; otherwise null. */
	paused: boolean | null;
};

/**
 * Resolve the CLI path from optional action settings.
 */
export function resolveCliPath(cliPath?: string): string {
	const trimmed = cliPath?.trim();
	return trimmed && trimmed.length > 0 ? trimmed : DEFAULT_CLI_PATH;
}

/** Global chain so at most one deskflow-cli.exe runs at a time (avoids IPC pile-ups). */
let cliQueue: Promise<void> = Promise.resolve();

function enqueueCli<T>(run: () => Promise<T>): Promise<T> {
	const next = cliQueue.then(run, run);
	// Keep the queue alive even if a run rejects; callers still get the rejection.
	cliQueue = next.then(
		() => undefined,
		() => undefined,
	);
	return next;
}

function killChild(child: ChildProcess): void {
	try {
		child.kill();
	} catch {
		// already exited
	}
	// Windows often needs taskkill for console children that ignore SIGTERM.
	if (process.platform === "win32" && child.pid) {
		try {
			spawn("taskkill", ["/PID", String(child.pid), "/T", "/F"], {
				windowsHide: true,
				stdio: "ignore",
			}).unref();
		} catch {
			// best-effort
		}
	}
}

/**
 * Run `deskflow-cli <command>` and map exit codes / stdout to pause state.
 * Invocations are serialized plugin-wide and hard-timed out.
 */
export async function runDeskflowCli(
	command: DeskflowCliCommand,
	cliPath?: string,
): Promise<DeskflowCliResult> {
	const exe = resolveCliPath(cliPath);

	try {
		await access(exe, fsConstants.F_OK);
	} catch {
		return {
			exitCode: 2,
			stdout: "",
			stderr: `deskflow-cli not found: ${exe}`,
			paused: null,
		};
	}

	return enqueueCli(
		() =>
			new Promise<DeskflowCliResult>((resolve) => {
				let settled = false;
				const settle = (result: DeskflowCliResult) => {
					if (settled) {
						return;
					}
					settled = true;
					clearTimeout(timer);
					resolve(result);
				};

				const child = spawn(exe, [command], {
					windowsHide: true,
					stdio: ["ignore", "pipe", "pipe"],
					// Avoid attaching a new console that can flash conhost.exe farms.
					detached: false,
				});

				let stdout = "";
				let stderr = "";

				child.stdout.setEncoding("utf8");
				child.stderr.setEncoding("utf8");
				child.stdout.on("data", (chunk: string) => {
					stdout += chunk;
				});
				child.stderr.on("data", (chunk: string) => {
					stderr += chunk;
				});

				const timer = setTimeout(() => {
					killChild(child);
					settle({
						exitCode: 2,
						stdout: stdout.trim(),
						stderr: stderr.trim() || `deskflow-cli ${command} timed out after ${CLI_TIMEOUT_MS}ms`,
						paused: null,
					});
				}, CLI_TIMEOUT_MS);
				timer.unref?.();

				child.on("error", (err) => {
					settle({
						exitCode: 2,
						stdout,
						stderr: err.message || String(err),
						paused: null,
					});
				});

				child.on("close", (code) => {
					const exitCode = code ?? 2;
					settle({
						exitCode,
						stdout: stdout.trim(),
						stderr: stderr.trim(),
						paused: pausedFromResult(exitCode, stdout),
					});
				});
			}),
	);
}

function pausedFromResult(exitCode: number, stdout: string): boolean | null {
	if (exitCode === 0) {
		return true;
	}
	if (exitCode === 1) {
		return false;
	}

	const match = /paused\s*=\s*(true|false)/i.exec(stdout);
	if (match) {
		return match[1].toLowerCase() === "true";
	}
	return null;
}

/** Map pause boolean to Stream Deck state index (0=Active, 1=Paused). */
export function stateFromPaused(paused: boolean): 0 | 1 {
	return paused ? 1 : 0;
}
