#!/usr/bin/env python3
"""Send a command to the MorphOS QEMU monitor (unix socket) and print the reply.
Usage: morphos-vm-mon.py "<monitor command>"
e.g.  morphos-vm-mon.py "screendump /tmp/m.ppm"
      morphos-vm-mon.py "sendkey ret"
      morphos-vm-mon.py "info status"
"""
import socket
import sys
import time

SOCK = ("/Volumes/EXT/Macchine Virtuali/Amiga/emu/telegram-amiga/"
        "morphos/qemu-monitor.sock")


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else "info status"
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(10)
    s.connect(SOCK)
    time.sleep(0.3)
    try:
        s.recv(65536)  # QEMU monitor banner
    except Exception:
        pass
    s.sendall((cmd + "\n").encode())
    time.sleep(0.6)
    out = b""
    try:
        while True:
            chunk = s.recv(65536)
            if not chunk:
                break
            out += chunk
            if len(chunk) < 65536:
                break
    except Exception:
        pass
    sys.stdout.write(out.decode(errors="replace"))
    s.close()


if __name__ == "__main__":
    main()
