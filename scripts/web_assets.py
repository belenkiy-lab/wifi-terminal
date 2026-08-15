# Fetch pinned third-party web assets before building the LittleFS image.
# The device serves these files locally; it does not need Internet access at runtime.

import base64
import hashlib
import io
import json
import os
import tarfile
import urllib.request

Import("env")

XTERM_VERSION = "5.5.0"
XTERM_TARBALL_URL = (
    "https://registry.npmjs.org/@xterm/xterm/-/xterm-5.5.0.tgz"
)
XTERM_TARBALL_SHA512 = (
    "hqJHYaQb5OptNunnyAnkHyM8aCjZ1MEIDTQu1iIbbTD/"
    "xops91NB5yq1ZK/dC2JDbVWtF23zUtl9JE2NqwT87A=="
)

FIT_ADDON_VERSION = "0.10.0"
FIT_ADDON_TARBALL_URL = (
    "https://registry.npmjs.org/@xterm/addon-fit/-/addon-fit-0.10.0.tgz"
)


def _write_if_changed(path, data):
    if os.path.isfile(path):
        with open(path, "rb") as existing:
            if existing.read() == data:
                return
    with open(path, "wb") as output:
        output.write(data)


def _download_xterm(data_dir):
    js_path = os.path.join(data_dir, "xterm.js")
    css_path = os.path.join(data_dir, "xterm.css")
    if os.path.isfile(js_path) and os.path.isfile(css_path):
        return

    print("Downloading xterm.js %s web assets..." % XTERM_VERSION)
    with urllib.request.urlopen(XTERM_TARBALL_URL, timeout=30) as response:
        archive = response.read()

    expected = base64.b64decode(XTERM_TARBALL_SHA512)
    actual = hashlib.sha512(archive).digest()
    if actual != expected:
        raise RuntimeError("xterm.js archive SHA-512 verification failed")

    with tarfile.open(fileobj=io.BytesIO(archive), mode="r:gz") as package:
        js_file = package.extractfile("package/lib/xterm.js")
        css_file = package.extractfile("package/css/xterm.css")
        if js_file is None or css_file is None:
            raise RuntimeError("xterm.js archive does not contain expected web assets")
        js = js_file.read()
        css = css_file.read()

    _write_if_changed(js_path, js)
    _write_if_changed(css_path, css)


def _download_fit_addon(data_dir):
    addon_path = os.path.join(data_dir, "addon-fit.js")
    if os.path.isfile(addon_path):
        return

    print("Downloading @xterm/addon-fit %s..." % FIT_ADDON_VERSION)
    with urllib.request.urlopen(FIT_ADDON_TARBALL_URL, timeout=30) as response:
        archive = response.read()

    with tarfile.open(fileobj=io.BytesIO(archive), mode="r:gz") as package:
        package_json_file = package.extractfile("package/package.json")
        addon_file = package.extractfile("package/lib/addon-fit.js")
        if package_json_file is None or addon_file is None:
            raise RuntimeError("xterm fit addon archive does not contain expected files")

        metadata = json.loads(package_json_file.read().decode("utf-8"))
        if metadata.get("name") != "@xterm/addon-fit" or metadata.get("version") != FIT_ADDON_VERSION:
            raise RuntimeError("unexpected xterm fit addon package metadata")

        addon = addon_file.read()

    _write_if_changed(addon_path, addon)


def ensure_xterm_assets(source, target, env):
    data_dir = env.subst("$PROJECT_DATA_DIR")
    os.makedirs(data_dir, exist_ok=True)

    _download_xterm(data_dir)
    _download_fit_addon(data_dir)
    print("xterm.js web assets ready")


# Hook the actual filesystem image file, not the buildfs alias. PlatformIO
# resolves the alias after its dependencies, so a pre-action on "buildfs"
# can run too late (after littlefs.bin has already been created).
env.AddPreAction("$BUILD_DIR/littlefs.bin", ensure_xterm_assets)
