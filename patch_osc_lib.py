"""
patch_osc_lib.py — PlatformIO extra_script

The CNMAT/OSC library ships SLIPEncodedBluetoothSerial.cpp which uses the
Classic BluetoothSerial API.  On ESP32-S3 (and any config without
CONFIG_BT_CLASSIC_ENABLED) that header is conditionally empty, so
'BluetoothSerial' is an undefined type and the file fails to compile.

This script replaces the file with a harmless stub the first time it is
seen, so the rest of the OSC library (OSCMessage, OSCBundle, …) builds fine.
The OSC *client* functionality we use (sendUDP / sendTCP) does not depend
on the Bluetooth serial wrapper at all.
"""

Import("env")  # noqa: F821  (PlatformIO injects this)
import os

bt_marker = "// stubbed-by-patch_osc_lib"

osc_dir = os.path.join(
    env.subst("$PROJECT_LIBDEPS_DIR"),
    env.subst("$PIOENV"),
    "OSC",
)

for fname in ("SLIPEncodedBluetoothSerial.cpp", "SLIPEncodedBluetoothSerial.h"):
    fpath = os.path.join(osc_dir, fname)
    if not os.path.exists(fpath):
        continue
    with open(fpath, "r") as fh:
        content = fh.read()
    if bt_marker in content:
        continue  # already patched
    with open(fpath, "w") as fh:
        fh.write(bt_marker + "\n")
        fh.write("// BluetoothSerial is not available on this platform.\n")
        fh.write("// The OSC client (UDP/TCP) does not require this file.\n")
    print("[patch_osc_lib] Stubbed out", fpath)
