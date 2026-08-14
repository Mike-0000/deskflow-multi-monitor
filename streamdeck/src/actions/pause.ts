import streamDeck, {
	action,
	type DidReceiveSettingsEvent,
	type KeyAction,
	type KeyDownEvent,
	SingletonAction,
	type WillAppearEvent,
	type WillDisappearEvent,
} from "@elgato/streamdeck";

import { runDeskflowCli, stateFromPaused } from "../deskflow-cli";

/** Per-action settings; `cliPath` overrides the default Program Files path. */
type PauseSettings = {
	cliPath?: string;
};

/** Non-overlapping poll cadence (next tick scheduled only after prior status finishes). */
const REFRESH_MS = 2000;

/**
 * Deskflow Pause — two-state key (0=Active, 1=Paused).
 * Presses toggle pause via deskflow-cli; appearance + poll keep state in sync with the GUI.
 */
@action({ UUID: "com.deskflow.control.pause" })
export class PauseAction extends SingletonAction<PauseSettings> {
	/** Per-context refresh timer (setTimeout chain, not setInterval). */
	private readonly refreshTimers = new Map<string, NodeJS.Timeout>();
	/** True while a status refresh is in flight for that context (coalesce). */
	private readonly statusInFlight = new Map<string, boolean>();
	/** True while a toggle is in flight for that context. */
	private readonly toggleInFlight = new Map<string, boolean>();
	/** Cached cliPath so polls skip getSettings WebSocket round-trips. */
	private readonly cliPathByContext = new Map<string, string | undefined>();
	/** Tracks last known reachability so poll only alerts on transition to offline. */
	private readonly online = new Map<string, boolean>();

	/**
	 * Sync state when the key appears on the canvas, then poll every ~2s.
	 */
	override async onWillAppear(ev: WillAppearEvent<PauseSettings>): Promise<void> {
		if (!ev.action.isKey()) {
			return;
		}
		this.cliPathByContext.set(ev.action.id, ev.payload.settings.cliPath);
		await this.refreshFromStatus(ev.action, true);
		this.startRefresh(ev.action);
	}

	/**
	 * Stop polling when the key leaves the canvas.
	 */
	override onWillDisappear(ev: WillDisappearEvent<PauseSettings>): void {
		this.stopRefresh(ev.action.id);
		this.statusInFlight.delete(ev.action.id);
		this.toggleInFlight.delete(ev.action.id);
		this.cliPathByContext.delete(ev.action.id);
		this.online.delete(ev.action.id);
	}

	/**
	 * Restart polling if the user changes the CLI path in Property Inspector.
	 */
	override async onDidReceiveSettings(
		ev: DidReceiveSettingsEvent<PauseSettings>,
	): Promise<void> {
		if (!ev.action.isKey()) {
			return;
		}
		this.cliPathByContext.set(ev.action.id, ev.payload.settings.cliPath);
		await this.refreshFromStatus(ev.action, true);
		this.startRefresh(ev.action);
	}

	/**
	 * Toggle pause on press; update state from CLI exit code; alert if core is down.
	 */
	override async onKeyDown(ev: KeyDownEvent<PauseSettings>): Promise<void> {
		if (this.toggleInFlight.get(ev.action.id)) {
			return;
		}

		this.toggleInFlight.set(ev.action.id, true);
		this.cliPathByContext.set(ev.action.id, ev.payload.settings.cliPath);

		try {
			const result = await runDeskflowCli("toggle", ev.payload.settings.cliPath);

			if (result.paused === null) {
				streamDeck.logger.error(
					`toggle failed: ${result.stderr || result.stdout || `exit ${result.exitCode}`}`,
				);
				this.online.set(ev.action.id, false);
				await ev.action.showAlert();
				return;
			}

			this.online.set(ev.action.id, true);
			await ev.action.setState(stateFromPaused(result.paused));
			await ev.action.showOk();
		} finally {
			this.toggleInFlight.set(ev.action.id, false);
		}
	}

	/**
	 * Schedule the next status poll after REFRESH_MS. Replaces any existing timer.
	 * Uses setTimeout (not setInterval) so ticks never overlap for one context.
	 */
	private startRefresh(action: KeyAction<PauseSettings>): void {
		this.stopRefresh(action.id);
		const timer = setTimeout(() => {
			this.refreshTimers.delete(action.id);
			void this.refreshFromStatus(action, false).finally(() => {
				// Only reschedule if this context is still supposed to be polling
				// (willDisappear clears the timer map entry and in-flight flag).
				if (this.cliPathByContext.has(action.id)) {
					this.startRefresh(action);
				}
			});
		}, REFRESH_MS);
		timer.unref?.();
		this.refreshTimers.set(action.id, timer);
	}

	private stopRefresh(actionId: string): void {
		const timer = this.refreshTimers.get(actionId);
		if (timer) {
			clearTimeout(timer);
			this.refreshTimers.delete(actionId);
		}
	}

	private async refreshFromStatus(
		action: KeyAction<PauseSettings>,
		alertOnError: boolean,
	): Promise<void> {
		if (this.statusInFlight.get(action.id)) {
			return;
		}
		this.statusInFlight.set(action.id, true);

		try {
			const cliPath =
				this.cliPathByContext.get(action.id) ??
				(await action.getSettings()).cliPath;
			this.cliPathByContext.set(action.id, cliPath);

			const result = await runDeskflowCli("status", cliPath);

			if (result.paused === null) {
				const wasOnline = this.online.get(action.id) !== false;
				this.online.set(action.id, false);
				streamDeck.logger.warn(
					`status failed: ${result.stderr || result.stdout || `exit ${result.exitCode}`}`,
				);
				if (alertOnError || wasOnline) {
					await action.showAlert();
				}
				return;
			}

			this.online.set(action.id, true);
			await action.setState(stateFromPaused(result.paused));
		} finally {
			if (this.cliPathByContext.has(action.id)) {
				this.statusInFlight.set(action.id, false);
			} else {
				this.statusInFlight.delete(action.id);
			}
		}
	}
}
