#!/usr/bin/env python3
"""Central Computer stand-in for the LNC binary TLV protocol.

Sends management commands and decodes the keep-alive / event / data / time frames
coming back. On start it sends the ASCII 'proto' line so the board flips into
binary protocol mode automatically (harmless if it's already in protocol mode).

Usage:  python3 central.py [/dev/cu.usbmodemXXXX]
At the prompt type 'help'. macOS: prefer the /dev/cu.* device and quit `screen` first.
No external packages needed (uses termios).
"""
import os, sys, termios, struct, time, glob, select

# ---- protocol tags (must match protocol.h) ----
CMD_SET_TEMP_NORMAL=0x20; CMD_SET_TEMP_WARNING=0x21
CMD_SET_HUM_NORMAL=0x22;  CMD_SET_HUM_WARNING=0x23
CMD_SET_LIGHT_NORMAL=0x24;CMD_SET_LIGHT_WARNING=0x25
CMD_SET_BATT_NORMAL=0x26; CMD_SET_BATT_WARNING=0x27
CMD_SET_RTC=0x28; CMD_GET_TIME=0x29
CMD_GET_DATA_RANGE=0x2A; CMD_GET_EVENTS_RANGE=0x2B
TAG_TS=0x10; TAG_MEAS=0x11; TAG_MODE=0x12; TAG_SRC=0x13
MODES={0:"NORMAL",1:"WARNING",2:"ERROR"}
SRCS ={0:"MONITOR",1:"OBJECT",2:"CONFIG",3:"INIT"}
TOP  ={1:"KEEP_ALIVE",2:"EVENT",3:"DATA",4:"TIME"}
MAXLEN=48

def tlv(tag, val=b''):
    return bytes([tag, len(val)]) + val

# ---- self-syncing frame decoder (shared with central loop) ----
def _children(v):
    d, i = {}, 0
    while i + 2 <= len(v):
        t, l = v[i], v[i+1]; d[t] = v[i+2:i+2+l]; i += 2 + l
    return d
def _children_ok(v):
    i = 0
    while i < len(v):
        if i + 2 > len(v): return False
        l = v[i+1]
        if i + 2 + l > len(v): return False
        i += 2 + l
    return i == len(v)
def fmt(tag, val):
    if tag in (1, 3):
        c = _children(val); ts = struct.unpack('<I', c.get(TAG_TS, b'\0\0\0\0'))[0]
        m = c.get(TAG_MEAS, b'\0'*8); t,h,l,b = struct.unpack('<hHHH', m[:8])
        mode = c.get(TAG_MODE, b'\0')[0]
        return f"{TOP[tag]:10} ts={ts} temp={t} hum={h} light={l} batt={b} mode={MODES.get(mode,mode)}"
    if tag == 2:
        c = _children(val); ts = struct.unpack('<I', c.get(TAG_TS, b'\0\0\0\0'))[0]
        src = c.get(TAG_SRC, b'\0')[0]; extra = ""
        if len(c.get(TAG_MODE, b'')) == 2:
            extra = f" {MODES.get(c[TAG_MODE][0])}->{MODES.get(c[TAG_MODE][1])}"
        return f"EVENT      ts={ts} src={SRCS.get(src,src)}{extra}"
    if tag == 4:
        ts = struct.unpack('<I', val[:4])[0] if len(val) >= 4 else 0
        return f"TIME       {ts}"
    return None
def decode(buf):
    out = []
    while True:
        while len(buf) >= 1 and buf[0] not in TOP: del buf[0]
        if len(buf) < 2: break
        tag, length = buf[0], buf[1]
        if length > MAXLEN: del buf[0]; continue
        if len(buf) < 2 + length: break
        val = bytes(buf[2:2+length])
        if tag in (1, 2, 3) and not _children_ok(val): del buf[0]; continue
        del buf[:2+length]; out.append(fmt(tag, val))
    return out

def open_port(port):
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    a = termios.tcgetattr(fd)
    a[0]=0; a[1]=0; a[2]=termios.CREAD|termios.CLOCAL|termios.CS8; a[3]=0
    a[4]=termios.B115200; a[5]=termios.B115200
    a[6][termios.VMIN]=0; a[6][termios.VTIME]=0
    termios.tcsetattr(fd, termios.TCSANOW, a)
    return fd

def pump(fd, buf, secs):
    end = time.time() + secs
    while time.time() < end:
        r,_,_ = select.select([fd], [], [], 0.2)
        if r:
            try: d = os.read(fd, 256)
            except BlockingIOError: d = b''
            if d:
                buf.extend(d)
                for line in decode(buf): print("  " + line)

HELP = ("  listen [secs]        watch keep-alive/event frames\n"
        "  gettime              request the RTC time (RSP_TIME)\n"
        "  time <epoch>         set the RTC\n"
        "  settn <lo> <hi>      set temp NORMAL range\n"
        "  settw <lo> <hi>      set temp WARNING range\n"
        "  sethn/setln/setbn <v>  set humidity/light/battery NORMAL lower bound\n"
        "  data <from> <to>     GET_DATA_RANGE (streams measurement records)\n"
        "  events <from> <to>   GET_EVENTS_RANGE (streams event records)\n"
        "  quit")

def main():
    port = sys.argv[1] if len(sys.argv) > 1 else next(iter(glob.glob('/dev/cu.usbmodem*')), None)
    if not port:
        print("no /dev/cu.usbmodem* found — pass the port as an argument"); return
    fd = open_port(port); buf = bytearray()
    print(f"Central on {port} @115200")
    os.write(fd, b"proto\r\n"); time.sleep(0.3); buf.clear()   # ensure protocol mode
    print("Board switched to protocol mode. Type 'help'. Incoming frames indent by 2 spaces.")
    try:
        while True:
            try: line = input("central> ").strip()
            except EOFError: break
            if not line:
                pump(fd, buf, 1.0); continue
            w = line.split(); c = w[0]
            try:
                if c in ("quit", "exit"): break
                elif c == "help": print(HELP); continue
                elif c == "listen": pump(fd, buf, float(w[1]) if len(w) > 1 else 8.0); continue
                elif c == "gettime": os.write(fd, tlv(CMD_GET_TIME))
                elif c == "time":    os.write(fd, tlv(CMD_SET_RTC, struct.pack('<I', int(w[1]))))
                elif c == "settn":   os.write(fd, tlv(CMD_SET_TEMP_NORMAL,  struct.pack('<hh', int(w[1]), int(w[2]))))
                elif c == "settw":   os.write(fd, tlv(CMD_SET_TEMP_WARNING, struct.pack('<hh', int(w[1]), int(w[2]))))
                elif c == "sethn":   os.write(fd, tlv(CMD_SET_HUM_NORMAL,   struct.pack('<H', int(w[1]))))
                elif c == "setln":   os.write(fd, tlv(CMD_SET_LIGHT_NORMAL, struct.pack('<H', int(w[1]))))
                elif c == "setbn":   os.write(fd, tlv(CMD_SET_BATT_NORMAL,  struct.pack('<H', int(w[1]))))
                elif c == "data":    os.write(fd, tlv(CMD_GET_DATA_RANGE,   struct.pack('<II', int(w[1]), int(w[2]))))
                elif c == "events":  os.write(fd, tlv(CMD_GET_EVENTS_RANGE, struct.pack('<II', int(w[1]), int(w[2]))))
                else: print("  ? type help"); continue
                pump(fd, buf, 1.3)
            except (IndexError, ValueError):
                print("  bad args — type help")
    finally:
        os.close(fd)

if __name__ == "__main__":
    main()
