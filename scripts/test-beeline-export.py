#!/usr/bin/env python3
"""Fast structural checks of the standalone ZIP; run after export-beeline.py."""

import json
import importlib.util
import os
from pathlib import Path
import subprocess
from urllib.parse import urlsplit
import unittest
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ZIP = ROOT / "generated-artifacts/beeline-apk.zip"
ANDROID_PROJECT = os.environ.get("BEELINE_VERIFY_DIR")


class StandaloneExportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.zip = zipfile.ZipFile(ZIP)
        cls.names = {n.removeprefix("beeline-apk/") for n in cls.zip.namelist()}

    @classmethod
    def tearDownClass(cls):
        cls.zip.close()

    def text(self, name):
        return self.zip.read("beeline-apk/" + name).decode()

    def obj(self, name):
        return json.loads(self.text(name))

    def test_self_contained_package(self):
        package = self.obj("package.json")
        self.assertEqual(package["name"], "beeline-apk")
        self.assertEqual(package["dependencies"]["@tanstack/react-query"], "^5.90.21")
        self.assertIn("expo", package["dependencies"])
        self.assertIn("react-native", package["dependencies"])
        self.assertIn("expo-system-ui", package["dependencies"])
        self.assertIn("typescript", package["devDependencies"])
        self.assertNotIn("references", self.obj("tsconfig.json"))
        for name in self.names:
            if name.endswith((".json", ".js", ".ts", ".tsx")):
                for token in ("workspace:", "catalog:", "REPLIT_", "@workspace/"):
                    self.assertNotIn(token, self.text(name), (name, token))
        for name in self.names:
            self.assertFalse(name.startswith(("node_modules/", "server/", "scripts/",
                                             ".replit-artifact/", "android/", "ios/")))
            self.assertNotIn(name, ("pnpm-lock.yaml", ".npmrc", "pnpm-workspace.yaml"))

    def test_independent_npm_lock(self):
        package, lock = self.obj("package.json"), self.obj("package-lock.json")
        self.assertEqual(lock["lockfileVersion"], 3)
        for key in ("dependencies", "devDependencies", "engines"):
            self.assertEqual(package[key], lock["packages"][""][key])
        for name, entry in lock["packages"].items():
            self.assertFalse(entry.get("link"), name)
            if "resolved" in entry:
                url = urlsplit(entry["resolved"])
                self.assertEqual(url.scheme, "https")
                self.assertEqual(url.netloc, "registry.npmjs.org")
        # These binaries were excluded by root overrides, not by this export.
        self.assertNotIn("overrides", package)
        self.assertNotIn("resolutions", package)
        for unused in ("expo-image-picker", "expo-blur", "expo-glass-effect",
                       "expo-image", "expo-linear-gradient", "expo-symbols",
                       "expo-web-browser", "react-native-svg", "zod", "zod-validation-error"):
            self.assertNotIn(unused, package["dependencies"])
        self.assertNotIn("expo-modules-core", package["dependencies"])
        self.assertIn("node_modules/expo-modules-core", lock["packages"])

    def test_private_registry_rejected_and_normalization_preserves_integrity(self):
        spec = importlib.util.spec_from_file_location("export_beeline",
                                                      ROOT / "scripts/export-beeline.py")
        exporter = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(exporter)
        lock = {"packages": {"node_modules/example": {
            "version": "1.0.0", "integrity": "sha512-example",
            "resolved": "http://package-firewall.replit.internal/npm/example/-/example-1.0.0.tgz",
        }}}
        with self.assertRaises(ValueError):
            exporter.validate_public_lock(lock)
        fixed = exporter.normalize_public_lock(lock)["packages"]["node_modules/example"]
        self.assertEqual(fixed["resolved"],
                         "https://registry.npmjs.org/example/-/example-1.0.0.tgz")
        self.assertEqual(fixed["version"], "1.0.0")
        self.assertEqual(fixed["integrity"], "sha512-example")
        fixed["resolved"] = "https://another-private-mirror.invalid/example.tgz"
        with self.assertRaises(ValueError):
            exporter.validate_public_lock(lock)

    def test_native_module_and_public_imports(self):
        self.assertIn("from 'expo';", self.text("modules/beeline-android/index.ts"))
        self.assertIn("require('expo/config-plugins')",
                      self.text("plugins/with-beeline-android.js"))
        self.assertIn("modules/beeline-android/android/build.gradle", self.names)
        for source in ("BeeLineAndroidModule.kt", "BeeLineNotificationListenerService.kt"):
            self.assertIn("modules/beeline-android/android/src/main/java/expo/modules/beeline/"
                          + source, self.names)
        self.assertEqual(self.obj("modules/beeline-android/expo-module.config.json")
                         ["android"]["modules"], ["expo.modules.beeline.BeeLineAndroidModule"])
        for filename in (".gitignore", ".easignore"):
            lines = self.text(filename).splitlines()
            self.assertIn("/android/", lines)
            self.assertNotIn("android/", lines)
        self.assertIn("lib/speed-task.android.ts", self.names)

    def test_android_and_eas_configuration(self):
        config = self.obj("app.json")["expo"]
        self.assertEqual(config["android"]["package"], "app.beeline.navigator")
        self.assertEqual(config["name"], "BeeLine Navigator")
        blocked = config["android"]["blockedPermissions"]
        self.assertIn("android.permission.RECORD_AUDIO", blocked)
        self.assertIn("android.permission.READ_EXTERNAL_STORAGE", blocked)
        location = next(p[1] for p in config["plugins"] if isinstance(p, list)
                        and p[0] == "expo-location")
        self.assertTrue(location["isAndroidBackgroundLocationEnabled"])
        self.assertTrue(location["isAndroidForegroundServiceEnabled"])
        eas = self.obj("eas.json")
        self.assertEqual(eas["cli"]["appVersionSource"], "local")
        self.assertEqual(eas["build"]["preview"]["android"]["buildType"], "apk")
        self.assertEqual(eas["build"]["preview"]["distribution"], "internal")
        self.assertEqual(eas["build"]["preview"]["node"], "22.13.0")
        self.assertNotIn("EAS_PNPM_FILTER_WORKSPACE", self.text("eas.json"))


@unittest.skipUnless(ANDROID_PROJECT, "Set BEELINE_VERIFY_DIR after prebuild/export")
class GeneratedAndroidTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.project = Path(ANDROID_PROJECT)
        cls.android = "{http://schemas.android.com/apk/res/android}"
        cls.tools = "{http://schemas.android.com/tools}"
        cls.manifest = ET.parse(
            cls.project / "android/app/src/main/AndroidManifest.xml").getroot()

    def test_generated_permissions(self):
        active = {e.get(self.android + "name")
                  for e in self.manifest.findall("uses-permission")
                  if e.get(self.tools + "node") != "remove"}
        required = {"BLUETOOTH_SCAN", "BLUETOOTH_CONNECT", "ACCESS_FINE_LOCATION",
                    "ACCESS_BACKGROUND_LOCATION", "FOREGROUND_SERVICE_LOCATION"}
        forbidden = {"RECORD_AUDIO", "READ_EXTERNAL_STORAGE", "WRITE_EXTERNAL_STORAGE",
                     "READ_MEDIA_IMAGES", "READ_MEDIA_VIDEO", "READ_MEDIA_AUDIO"}
        self.assertTrue({"android.permission." + p for p in required} <= active)
        self.assertFalse({"android.permission." + p for p in forbidden} & active)
        service = next(e for e in self.manifest.find("application").findall("service")
                       if e.get(self.android + "name") ==
                       "expo.modules.beeline.BeeLineNotificationListenerService")
        self.assertEqual(service.get(self.android + "permission"),
                         "android.permission.BIND_NOTIFICATION_LISTENER_SERVICE")
        self.assertEqual(service.get(self.android + "exported"), "true")
        self.assertEqual(service.find("intent-filter/action").get(self.android + "name"),
                         "android.service.notification.NotificationListenerService")

    def test_native_autolinking(self):
        data = json.loads(subprocess.check_output(
            ["npx", "--no-install", "expo-modules-autolinking", "resolve",
             "--platform", "android", "--json"], cwd=self.project, text=True))
        modules = [m for m in data["modules"] if m["packageName"] == "beeline-android"]
        self.assertEqual(len(modules), 1)
        self.assertTrue(any(m["classifier"] == "expo.modules.beeline.BeeLineAndroidModule"
                            for p in modules[0]["projects"] for m in p["modules"]))
        source = self.project / "modules/beeline-android/android"
        self.assertEqual(Path(modules[0]["projects"][0]["sourceDir"]), source)

    def test_application_id_and_android_bundle(self):
        gradle = (self.project / "android/app/build.gradle").read_text()
        self.assertIn("applicationId 'app.beeline.navigator'", gradle)
        self.assertTrue(list((self.project / "bundle-check/_expo/static/js/android")
                             .glob("*.hbc")))


if __name__ == "__main__":
    unittest.main(verbosity=2)
