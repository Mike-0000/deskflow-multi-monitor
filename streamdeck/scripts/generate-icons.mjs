/**
 * Generates minimal PNG icons for the Deskflow Stream Deck plugin (no native deps).
 * Sizes follow Elgato guidelines: key 72/144, action icon 20/40, category 28/56, marketplace 288.
 */
import { deflateSync } from "node:zlib";
import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "com.deskflow.control.sdPlugin", "imgs");

/** @typedef {{ r: number, g: number, b: number, a?: number }} Rgba */

/**
 * @param {number} width
 * @param {number} height
 * @param {(x: number, y: number) => Rgba} paint
 */
function encodePng(width, height, paint) {
	const raw = Buffer.alloc((width * 4 + 1) * height);
	for (let y = 0; y < height; y++) {
		const row = y * (width * 4 + 1);
		raw[row] = 0;
		for (let x = 0; x < width; x++) {
			const { r, g, b, a = 255 } = paint(x, y);
			const i = row + 1 + x * 4;
			raw[i] = r;
			raw[i + 1] = g;
			raw[i + 2] = b;
			raw[i + 3] = a;
		}
	}

	const compressed = deflateSync(raw, { level: 9 });
	const signature = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);
	const ihdr = chunk(
		"IHDR",
		Buffer.from([
			(width >>> 24) & 0xff,
			(width >>> 16) & 0xff,
			(width >>> 8) & 0xff,
			width & 0xff,
			(height >>> 24) & 0xff,
			(height >>> 16) & 0xff,
			(height >>> 8) & 0xff,
			height & 0xff,
			8,
			6,
			0,
			0,
			0,
		]),
	);
	const idat = chunk("IDAT", compressed);
	const iend = chunk("IEND", Buffer.alloc(0));
	return Buffer.concat([signature, ihdr, idat, iend]);
}

/**
 * @param {string} type
 * @param {Buffer} data
 */
function chunk(type, data) {
	const typeBuf = Buffer.from(type, "ascii");
	const len = Buffer.alloc(4);
	len.writeUInt32BE(data.length, 0);
	const crc = Buffer.alloc(4);
	crc.writeUInt32BE(crc32(Buffer.concat([typeBuf, data])), 0);
	return Buffer.concat([len, typeBuf, data, crc]);
}

/** @param {Buffer} buf */
function crc32(buf) {
	let c = ~0;
	for (let i = 0; i < buf.length; i++) {
		c ^= buf[i];
		for (let k = 0; k < 8; k++) {
			c = c & 1 ? (0xedb88320 ^ (c >>> 1)) : c >>> 1;
		}
	}
	return ~c >>> 0;
}

/**
 * Rounded rect filled with color; optional center glyph.
 * @param {number} size
 * @param {Rgba} bg
 * @param {'play' | 'pause' | 'mark'} glyph
 * @param {Rgba} fg
 */
function drawIcon(size, bg, glyph, fg) {
	const r = Math.max(4, Math.round(size * 0.16));
	const cx = (size - 1) / 2;
	const cy = (size - 1) / 2;

	return encodePng(size, size, (x, y) => {
		const inside =
			x >= r &&
			x < size - r &&
			y >= r &&
			y < size - r
				? true
				: dist(x, y, r, r) <= r ||
					dist(x, y, size - 1 - r, r) <= r ||
					dist(x, y, r, size - 1 - r) <= r ||
					dist(x, y, size - 1 - r, size - 1 - r) <= r ||
					(x >= r && x < size - r && y >= 0 && y < size) ||
					(y >= r && y < size - r && x >= 0 && x < size);

		if (!inside) {
			return { r: 0, g: 0, b: 0, a: 0 };
		}

		if (glyph === "play") {
			// Right-pointing triangle (Active / sharing)
			const left = size * 0.34;
			const right = size * 0.68;
			const top = size * 0.28;
			const bot = size * 0.72;
			const t = (y - top) / (bot - top);
			if (t >= 0 && t <= 1) {
				const maxX = left + (right - left) * (1 - Math.abs(2 * t - 1));
				if (x >= left && x <= maxX) {
					return fg;
				}
			}
		} else if (glyph === "pause") {
			const barW = size * 0.12;
			const gap = size * 0.1;
			const left1 = size * 0.32;
			const left2 = left1 + barW + gap;
			const top = size * 0.28;
			const bot = size * 0.72;
			if (y >= top && y <= bot) {
				if ((x >= left1 && x <= left1 + barW) || (x >= left2 && x <= left2 + barW)) {
					return fg;
				}
			}
		} else {
			// Simple "D" mark for category / marketplace
			const ox = size * 0.28;
			const oy = size * 0.22;
			const w = size * 0.44;
			const h = size * 0.56;
			const thickness = Math.max(2, size * 0.1);
			const inVert = x >= ox && x <= ox + thickness && y >= oy && y <= oy + h;
			const inTop = y >= oy && y <= oy + thickness && x >= ox && x <= ox + w * 0.55;
			const inBot = y >= oy + h - thickness && y <= oy + h && x >= ox && x <= ox + w * 0.55;
			const rx = ox + w * 0.35;
			const ry = oy + h / 2;
			const rr = h * 0.42;
			const angOk = x >= rx;
			const ring = Math.abs(dist(x, y, rx, ry) - rr) <= thickness * 0.65 && angOk;
			if (inVert || inTop || inBot || ring) {
				return fg;
			}
		}

		return bg;
	});
}

/** @param {number} x @param {number} y @param {number} cx @param {number} cy */
function dist(x, y, cx, cy) {
	return Math.hypot(x - cx, y - cy);
}

/**
 * @param {string} rel
 * @param {Buffer} data
 */
function write(rel, data) {
	const full = join(root, rel);
	mkdirSync(dirname(full), { recursive: true });
	writeFileSync(full, data);
	console.log("wrote", rel, data.length, "bytes");
}

const activeBg = { r: 34, g: 120, b: 78 };
const pausedBg = { r: 160, g: 90, b: 28 };
const brandBg = { r: 28, g: 36, b: 48 };
const fg = { r: 245, g: 248, b: 250 };

for (const size of [72, 144]) {
	const suffix = size === 72 ? "" : "@2x";
	write(`actions/pause/key-active${suffix}.png`, drawIcon(size, activeBg, "play", fg));
	write(`actions/pause/key-paused${suffix}.png`, drawIcon(size, pausedBg, "pause", fg));
}

for (const size of [20, 40]) {
	const suffix = size === 20 ? "" : "@2x";
	write(`actions/pause/icon${suffix}.png`, drawIcon(size, brandBg, "pause", fg));
}

for (const size of [28, 56]) {
	const suffix = size === 28 ? "" : "@2x";
	write(`plugin/category-icon${suffix}.png`, drawIcon(size, brandBg, "mark", fg));
}

write("plugin/marketplace.png", drawIcon(288, brandBg, "mark", fg));
write("plugin/marketplace@2x.png", drawIcon(576, brandBg, "mark", fg));

console.log("icons ready");
