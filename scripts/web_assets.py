# Fetch pinned third-party web assets before building the LittleFS image.
# The device serves these files locally; it does not need Internet access at runtime.

import base64
import hashlib
import io
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


def _write_if_changed(path, data):
    if os.path.isfile(path):
        with open(path, "rb") as existing:
            if existing.read() == data:
                return
    with open(path, "wb") as output:
        output.write(data)


def ensure_xterm_assets(source, target, env):
    data_dir = env.subst("$PROJECT_DATA_DIR")
    js_path = os.path.join(data_dir, "xterm.js")
    css_path = os.path.join(data_dir, "xterm.css")

    if os.path.isfile(js_path) and os.path.isfile(css_path):
        print("xterm.js web assets already present")
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

    os.makedirs(data_dir, exist_ok=True)
    _write_if_changed(js_path, js)
    _write_if_changed(css_path, css)
    print("xterm.js web assets ready")


# Hook the actual filesystem image file, not the buildfs alias. PlatformIO
# resolves the alias after its dependencies, so a pre-action on "buildfs"
# can run too late (after littlefs.bin has already been created).
env.AddPreAction("$BUILD_DIR/littlefs.bin", ensure_xterm_assets)
