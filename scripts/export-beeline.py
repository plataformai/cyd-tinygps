#!/usr/bin/env python3
"""Export BeeLine from tracked sources without copying workspace infrastructure.

Run: python3 scripts/export-beeline.py
Update the independent npm lock after a dependency change:
     python3 scripts/export-beeline.py --refresh-lock
"""

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
from urllib.parse import urlsplit, urlunsplit
import zipfile

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "artifacts/beeline-navigator"
TEMPLATES = ROOT / "scripts/beeline-export"
OUTPUT = ROOT / "generated-artifacts"
STAGE = OUTPUT / "beeline-apk"
LOCK = TEMPLATES / "package-lock.json"
COPY_DIRS = ("app", "assets", "components", "constants", "hooks", "lib", "modules", "plugins")
COPY_FILES = ("metro.config.js", ".gitignore")

# Audited in source code, app plugins, and Expo peer dependencies. expo-symbols
# is an optional Router peer, not required by this app's Stack-only navigation.
REMOVE = {
    "@workspace/api-client-react", "zod", "zod-validation-error", "create-launch",
    "expo-modules-core", "expo-image-picker", "expo-blur", "expo-glass-effect",
    "expo-image", "expo-linear-gradient", "expo-symbols",
    "expo-web-browser", "react-native-svg", "@stardazed/streams-text-encoding",
    "@ungap/structured-clone",
}
DEV = {"@babel/core", "@types/react", "@types/react-dom", "typescript",
       "babel-plugin-react-compiler"}


def read_json(path):
    return json.loads(path.read_text())


def write_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n")


def normalize_public_lock(lock):
    """Keep versions/integrities approved during resolution; export public URLs.

    The workspace registry can proxy tarballs even in an isolated directory.
    That proxy is for local package management, never an external build input.
    """
    for entry in lock["packages"].values():
        if "resolved" not in entry:
            continue
        url = urlsplit(entry["resolved"])
        if url.hostname == "package-firewall.replit.internal" and url.path.startswith("/npm/"):
            entry["resolved"] = urlunsplit(
                ("https", "registry.npmjs.org", url.path.removeprefix("/npm"), "", ""))
    validate_public_lock(lock)
    return lock


def validate_public_lock(lock):
    for name, entry in lock["packages"].items():
        if entry.get("link"):
            raise ValueError(f"Linked package is not portable: {name}")
        if "resolved" in entry:
            url = urlsplit(entry["resolved"])
            if (url.scheme != "https" or url.netloc != "registry.npmjs.org"
                    or url.username or url.password or url.query or url.fragment):
                raise ValueError(f"Non-public registry URL in npm lock: {name}")


def manifest():
    source = read_json(APP / "package.json")
    all_deps = {**source.get("dependencies", {}), **source.get("devDependencies", {})}
    all_deps = {name: version for name, version in all_deps.items() if name not in REMOVE}
    all_deps["@tanstack/react-query"] = "^5.90.21"
    # Patch alignment inside SDK 58, verified by `expo install --check` on
    # the independent lock (Expo 58.0.22). No Expo SDK migration.
    all_deps["react-native"] = "0.88.1"
    all_deps["@types/react-dom"] = "~19.3.0"
    for name, version in all_deps.items():
        if version.startswith(("workspace:", "catalog:", "file:", "link:")):
            raise ValueError(f"Unresolved local dependency: {name}: {version}")
    return {
        "name": "beeline-apk",
        "version": read_json(APP / "app.json")["expo"]["version"],
        "private": True,
        "main": source["main"],
        "engines": {"node": "^22.13.0 || ^24.3.0 || >=26.0.0"},
        "packageManager": "npm@10.9.2",
        "scripts": {
            "start": "expo start", "android": "expo run:android",
            "web": "expo start --web",
            "prebuild:android": "expo prebuild --platform android",
            "typecheck": "tsc -p tsconfig.json --noEmit",
        },
        "dependencies": dict(sorted((n, v) for n, v in all_deps.items() if n not in DEV)),
        "devDependencies": dict(sorted((n, v) for n, v in all_deps.items() if n in DEV)),
    }


def refresh_lock(package):
    # Outside the repository: npm must not discover its pnpm-only preinstall,
    # Linux-specific binary overrides, workspace catalog or existing node_modules.
    with tempfile.TemporaryDirectory(prefix="beeline-lock-") as directory:
        write_json(Path(directory) / "package.json", package)
        subprocess.run(
            ["npm", "install", "--package-lock-only", "--ignore-scripts",
             "--no-audit", "--no-fund"], cwd=directory, check=True,
        )
        write_json(LOCK, normalize_public_lock(
            read_json(Path(directory) / "package-lock.json")))


def export(refresh=False):
    package = manifest()
    if refresh:
        refresh_lock(package)
    if not LOCK.exists():
        raise SystemExit("Independent npm lock missing; run again with --refresh-lock.")
    lock = read_json(LOCK)
    validate_public_lock(lock)
    locked_root = lock["packages"][""]
    for key in ("dependencies", "devDependencies", "engines"):
        if package[key] != locked_root.get(key):
            raise SystemExit(f"Stale npm lock ({key}); use --refresh-lock.")
    if lock["name"] != package["name"] or lock["version"] != package["version"]:
        raise SystemExit("Stale npm lock identity; use --refresh-lock.")
    for directory in COPY_DIRS:
        if not (APP / directory).is_dir():
            raise SystemExit(f"Missing required source directory: {directory}")

    OUTPUT.mkdir(exist_ok=True)
    if STAGE.exists():
        shutil.rmtree(STAGE)
    STAGE.mkdir()
    for directory in COPY_DIRS:
        shutil.copytree(APP / directory, STAGE / directory,
                        ignore=shutil.ignore_patterns(
                            "node_modules", ".expo", "build", ".gradle", "*.tsbuildinfo",
                            "*.jks", "*.keystore", ".env", ".env.*", ".DS_Store"))
    for filename in COPY_FILES:
        shutil.copy2(APP / filename, STAGE / filename)
    # Expo generates this ignored file locally; a clean checkout may not have it.
    (STAGE / "expo-env.d.ts").write_text('/// <reference types="expo/types" />\n')
    write_json(STAGE / "package.json", package)
    config = read_json(APP / "app.json")
    config["expo"]["android"]["package"] = "app.beeline.navigator"
    config["expo"]["android"]["blockedPermissions"] = [
        "android.permission.RECORD_AUDIO",
        "android.permission.READ_EXTERNAL_STORAGE", "android.permission.WRITE_EXTERNAL_STORAGE",
        "android.permission.READ_MEDIA_IMAGES", "android.permission.READ_MEDIA_VIDEO",
        "android.permission.READ_MEDIA_AUDIO",
    ]
    write_json(STAGE / "app.json", config)
    tsconfig = read_json(APP / "tsconfig.json")
    tsconfig.pop("references", None)
    write_json(STAGE / "tsconfig.json", tsconfig)
    local_package = STAGE / "modules/beeline-android/package.json"
    local_module = read_json(local_package)
    local_module["name"] = "beeline-android"
    write_json(local_package, local_module)
    shutil.copy2(LOCK, STAGE / "package-lock.json")
    for filename in ("README.md", "eas.json", ".easignore", ".nvmrc"):
        shutil.copy2(TEMPLATES / filename, STAGE / filename)

    for path in STAGE.rglob("*"):
        if path.is_file() and path.suffix in (".json", ".js", ".ts", ".tsx"):
            text = path.read_text()
            if any(token in text for token in ("workspace:", "catalog:", "REPLIT_",
                                               "EAS_PNPM_FILTER_WORKSPACE", "@workspace/",
                                               "replit.internal")):
                raise SystemExit(f"Workspace-only reference leaked into export: {path}")
    target = OUTPUT / "beeline-apk.zip"
    # Stable member order/timestamps; the same source and lock yield the same ZIP.
    with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(STAGE.rglob("*")):
            if path.is_file():
                name = "beeline-apk/" + path.relative_to(STAGE).as_posix()
                info = zipfile.ZipInfo(name, (2026, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o100644 << 16
                archive.writestr(info, path.read_bytes())
    print(f"Created {target.relative_to(ROOT)} ({target.stat().st_size:,} bytes)")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--refresh-lock", action="store_true")
    parser.add_argument("--normalize-lock", action="store_true",
                        help="Normalize cached registry URLs without changing any versions")
    args = parser.parse_args()
    if args.normalize_lock:
        write_json(LOCK, normalize_public_lock(read_json(LOCK)))
    export(args.refresh_lock)
