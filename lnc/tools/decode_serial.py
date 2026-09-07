#!/usr/bin/env python3
"""Self-synchronizing TLV decoder for the LNC serial stream — a stand-in for
the Central Computer's receiver. Prints keep-alives, events, data reports and
time replies. Robust to starting mid-stream and to stray bytes: it validates
each frame's nested structure and slides one byte on any mismatch.

Usage:  python3 decode_serial.py [/dev/cu.usbmodemXXXX]
Prefer the /dev/cu.* device on macOS. Quit any `screen`/reader on the port first.
No external packages needed (uses termios).
"""
import os, sys, termios, struct

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/cu.usbmodem144203"

# Open first, THEN set baud on the fd (macOS resets a serial port to 9600 on open).
fd = os.open(PORT, os.O_RDONLY | os.O_NOCTTY)
a = termios.tcgetattr(fd)
a[0] = 0; a[1] = 0
a[2] = termios.CREAD | termios.CLOCAL | termios.CS8
a[3] = 0
a[4] = termios.B115200; a[5] = termios.B115200
a[6][termios.VMIN] = 1; a[6][termios.VTIME] = 0
termios.tcsetattr(fd, termios.TCSANOW, a)

MODES = {0: "NORMAL", 1: "WARNING", 2: "ERROR"}
SRCS  = {0: "MONITOR", 1: "OBJECT", 2: "CONFIG", 3: "INIT"}
TOP   = {1: "KEEP_ALIVE", 2: "EVENT", 3: "DATA", 4: "TIME"}
MAXLEN = 40   # no legitimate LNC frame value exceeds this

def children_ok(val):
    """True iff val is a sequence of TLV children that exactly fills it."""
    i = 0
    while i < len(val):
        if i + 2 > len(val):
            return False
        l = val[i + 1]
        if i + 2 + l > len(val):
            return False
        i += 2 + l
    return i == len(val)

def children(val):
    d, i = {}, 0
    while i + 2 <= len(val):
        t, l = val[i], val[i + 1]
        d[t] = val[i + 2:i + 2 + l]
        i += 2 + l
    return d

def emit(tag, val):
    if tag in (1, 3):
        c = children(val)
        ts = struct.unpack("<I", c.get(0x10, b"\0\0\0\0"))[0]
        m = c.get(0x11, b"\0" * 8)
        temp, hum, light, batt = struct.unpack("<hHHH", m[:8])
        mode = c.get(0x12, b"\0")[0]
        print(f"{TOP[tag]:10} ts={ts} temp={temp} hum={hum} "
              f"light={light} batt={batt} mode={MODES.get(mode, mode)}", flush=True)
    elif tag == 2:
        c = children(val)
        ts = struct.unpack("<I", c.get(0x10, b"\0\0\0\0"))[0]
        src = c.get(0x13, b"\0")[0]
        extra = ""
        if len(c.get(0x12, b"")) == 2:
            extra = f" {MODES.get(c[0x12][0])}->{MODES.get(c[0x12][1])}"
        print(f"EVENT      ts={ts} src={SRCS.get(src, src)}{extra}", flush=True)
    elif tag == 4:
        ts = struct.unpack("<I", val[:4])[0] if len(val) >= 4 else 0
        print(f"TIME       {ts}", flush=True)

buf = bytearray()
print(f"Listening on {PORT} @115200 — decoding TLV frames (Ctrl-C to stop)\n", flush=True)
while True:
    # Need at least a header to decide.
    while len(buf) < 2:
        buf += os.read(fd, 256)
    tag, length = buf[0], buf[1]
    if tag not in TOP or length > MAXLEN:
        del buf[0]                      # not a valid header here — slide one byte
        continue
    while len(buf) < 2 + length:        # wait for the full frame
        buf += os.read(fd, 256)
    val = bytes(buf[2:2 + length])
    if tag in (1, 2, 3) and not children_ok(val):
        del buf[0]                      # header looked valid but body isn't — slide
        continue
    emit(tag, val)
    del buf[:2 + length]                # consume the frame
