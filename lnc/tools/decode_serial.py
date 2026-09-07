#!/usr/bin/env python3
"""Minimal TLV decoder for the LNC serial stream — a stand-in for the Central
Computer's receiver. Prints keep-alives, events, data reports and time replies.

Usage:  python3 decode_serial.py [/dev/cu.usbmodemXXXX]
Prefer the /dev/cu.* device on macOS. Quit any `screen` session on the port first.
No external packages needed (uses termios).

NOTE: the baud MUST be set on the already-open fd (macOS resets a serial port to
9600 on open), so we open first, then tcsetattr to 115200 raw.
"""
import os, sys, termios, struct

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/cu.usbmodem144203"

fd = os.open(PORT, os.O_RDONLY | os.O_NOCTTY)
a = termios.tcgetattr(fd)
a[0] = 0                                   # iflag: raw
a[1] = 0                                   # oflag: raw
a[2] = termios.CREAD | termios.CLOCAL | termios.CS8
a[3] = 0                                   # lflag: non-canonical, no echo
a[4] = termios.B115200                     # ispeed
a[5] = termios.B115200                     # ospeed
a[6][termios.VMIN] = 1                     # block until >=1 byte
a[6][termios.VTIME] = 0
termios.tcsetattr(fd, termios.TCSANOW, a)

MODES = {0: "NORMAL", 1: "WARNING", 2: "ERROR"}
SRCS  = {0: "MONITOR", 1: "OBJECT", 2: "CONFIG", 3: "INIT"}
TOP   = {1: "KEEP_ALIVE", 2: "EVENT", 3: "DATA", 4: "TIME"}

def rd(n):
    b = b""
    while len(b) < n:
        c = os.read(fd, n - len(b))
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

print(f"Listening on {PORT} @115200 — decoding TLV frames (Ctrl-C to stop)\n", flush=True)
while True:
    tag = rd(1)[0]
    if tag not in TOP:
        continue                           # resync to the next known top-level tag
    length = rd(1)[0]
    val = rd(length)
    if tag in (1, 3):                      # keep-alive / data report (nested)
        c = children(val)
        ts = struct.unpack("<I", c.get(0x10, b"\0\0\0\0"))[0]
        m = c.get(0x11, b"\0" * 8)
        temp, hum, light, batt = struct.unpack("<hHHH", m[:8])
        mode = c.get(0x12, b"\0")[0]
        print(f"{TOP[tag]:10} ts={ts} temp={temp} hum={hum} "
              f"light={light} batt={batt} mode={MODES.get(mode, mode)}", flush=True)
    elif tag == 2:                         # event record
        c = children(val)
        ts = struct.unpack("<I", c.get(0x10, b"\0\0\0\0"))[0]
        src = c.get(0x13, b"\0")[0]
        extra = ""
        if len(c.get(0x12, b"")) == 2:
            extra = f" {MODES.get(c[0x12][0])}->{MODES.get(c[0x12][1])}"
        print(f"EVENT      ts={ts} src={SRCS.get(src, src)}{extra}", flush=True)
    elif tag == 4:                         # RSP_TIME (flat 4-byte epoch)
        ts = struct.unpack("<I", val[:4])[0] if len(val) >= 4 else 0
        print(f"TIME       {ts}", flush=True)
