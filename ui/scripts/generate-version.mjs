import { execSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const __dirname = dirname(fileURLToPath(import.meta.url));
const root = join(__dirname, "..", "..");
const outDir = join(__dirname, "..", "src-tauri");
const outFile = join(outDir, "version.json");

function git(cmd) {
  try {
    return execSync(cmd, { cwd: root, encoding: "utf8" }).trim();
  } catch {
    return "";
  }
}

const sha = git("git rev-parse --short=8 HEAD") || "unknown";
let version = "1.26.0.9999";
const describe = git("git describe --long --match v* --always");
const match = describe.match(/v?(\d+)\.(\d+)\.(\d+)-(\d+)-/);
if (match) {
  version = `${match[1]}.${match[2]}.${match[3]}.${match[4]}`;
}

const payload = {
  version,
  gitSha: sha,
  versionId: `${version}+${sha}`,
  coreIpcName: "deskflow-core",
  daemonIpcName: "deskflow-daemon",
  guiIpcName: "deskflow-gui",
  coreBinName: "deskflow-core",
  appName: "Deskflow",
  appId: "deskflow",
};

const next = JSON.stringify(payload, null, 2) + "\n";
mkdirSync(outDir, { recursive: true });

if (existsSync(outFile) && readFileSync(outFile, "utf8") === next) {
  console.log(`version.json unchanged: ${payload.versionId}`);
  process.exit(0);
}

writeFileSync(outFile, next);
console.log(`Generated version.json: ${payload.versionId}`);
