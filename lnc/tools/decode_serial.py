#!/usr/bin/env python3
"""Minimal TLV decoder for the LNC serial stream — a stand-in for the Central
Computer's receiver. Prints keep-alives, events, data reports and time replies
in human-readable form.

Usage:  python3 decode_serial.py [/dev/tty.usbmodemXXXX]
Quit the `screen` session first (Ctrl-A then K, confirm y) so the port is free.
No external packages needed.
"""
import sys, os, subprocess, struct

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/tty.usbmodem144203"

# Put the serial port into raw mode at 115200 (macOS/BSD stty).
subprocess.run(["stty", "-f", PORT, "115200", "raw", "-echo", "-ixon"], check=True)

MODES = {0: "NORMAL", 1: "WARNING", 2: "ERROR"}
SRCS  = {0: "MONITOR", 1: "OBJECT", 2: "CONFIG", 3: "INIT"}
TOP   = {1: "KEEP_ALIVE", 2: "EVENT", 3: "DATA", 4: "TIME"}

f = open(PORT, "rb", buffering=0)

def rd(n):
    b = b""
    while len(b) < n:
        c = f.read(n - len(b))
        if c:
            b += c
    return b

def children(val):
    d, i = {}, 0
    while i + 2 <= len(val):
        t, l = val[i], val[i + 1]
        d[t] = val[i + 2:i + 2 + l]
        i += 2 + l
    return d

print(f"Listening on {PORT} @115200 — decoding TLV frames (Ctrl-C to stop)\n")
while True:
    tag = rd(1)[0]
    if tag not in TOP:
        continue                      # resync to the next known top-level tag
    length = rd(1)[0]
    val = rd(length)
    if tag in (1, 3):                 # keep-alive / data report (nested)
        c = children(val)
        ts = struct.unpack("<I", c.get(0x10, b"\0\0\0\0"))[0]
        m = c.get(0x11, b"\0" * 8)
        temp, hum, light, batt = struct.unpack("<hHHH", m[:8])
        mode = c.get(0x12, b"\0")[0]
        print(f"{TOP[tag]:10} ts={ts} temp={temp} hum={hum} "
              f"light={light} batt={batt} mode={MODES.get(mode, mode)}")
    elif tag == 2:                     # event record
        c = children(val)
        ts = struct.unpack("<I", c.get(0x10, b"\0\0\0\0"))[0]
        src = c.get(0x13, b"\0")[0]
        extra = ""
        if len(c.get(0x12, b"")) == 2:
            extra = f" {MODES.get(c[0x12][0])}->{MODES.get(c[0x12][1])}"
        print(f"EVENT      ts={ts} src={SRCS.get(src, src)}{extra}")
    elif tag == 4:                     # RSP_TIME (flat: 4-byte epoch)
        ts = struct.unpack("<I", val[:4])[0] if len(val) >= 4 else 0
        print(f"TIME       {ts}")
