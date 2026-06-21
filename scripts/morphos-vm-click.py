#!/usr/bin/env python3
"""Click at a SCREEN coordinate (as seen in screendump, 1920x1080) on the MorphOS VM.
Converts screen->vncdo via the calibrated linear map, releases stuck modifiers,
moves + clicks, then screendumps to /tmp/vm.png (+ optional crop).

Calibration (verified 2026-06-18, 3 clean points): the vncdo framebuffer and the
screendump are both 1920x1080 but the pointer maps with scale ~1.111:
  measured_screen_x = 1.111*req_x - 106.8   ->  req_x = 0.900*screen_x + 96
  measured_screen_y = 1.11 *req_y - 59.5    ->  req_y = 0.901*screen_y + 54

Usage:
  morphos-vm-click.py X Y                 # left click at screen (X,Y)
  morphos-vm-click.py X Y --right         # menu RMB hold not handled here
  morphos-vm-click.py X Y --double
  morphos-vm-click.py X Y --crop x0 y0 x1 y1   # crop region for /tmp/vm_c.png
"""
import subprocess, socket, sys, time

VNCDO = "/Users/kaffeine/amiga-dev/.venv-vnc/bin/vncdo"
VNC = "127.0.0.1::5907"
MON = ("/Volumes/EXT/Macchine Virtuali/Amiga/emu/telegram-amiga/"
       "morphos/qemu-monitor.sock")


def req(sx, sy):
    return round(0.900 * sx + 96), round(0.901 * sy + 54)


def mon(cmd):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(10); s.connect(MON); time.sleep(0.2)
    try: s.recv(65536)
    except Exception: pass
    s.sendall((cmd + "\n").encode()); time.sleep(0.5)
    try: s.recv(65536)
    except Exception: pass
    s.close()


def vnc(*args):
    subprocess.run([VNCDO, "-s", VNC, *map(str, args)], capture_output=True)


def main():
    a = sys.argv[1:]
    sx = int(a[0]); sy = int(a[1]); a = a[2:]
    button = 1; double = False; crop = None
    while a:
        f = a.pop(0)
        if f == "--right": button = 3
        elif f == "--double": double = True
        elif f == "--crop":
            crop = tuple(int(a.pop(0)) for _ in range(4))
        else: break
    rx, ry = req(sx, sy)
    vnc("keyup", "shift", "keyup", "ctrl", "keyup", "alt")
    if double:
        vnc("move", rx, ry, "pause", "0.2", "click", button,
            "pause", "0.15", "click", button)
    else:
        vnc("move", rx, ry, "pause", "0.3", "click", button)
    vnc("keyup", "shift", "keyup", "ctrl")
    time.sleep(1.5)
    mon('screendump "/tmp/vm.ppm"')
    subprocess.run(["sips", "-s", "format", "png", "/tmp/vm.ppm",
                    "--out", "/tmp/vm.png"], capture_output=True)
    if crop:
        try:
            from PIL import Image
            Image.open("/tmp/vm.png").crop(crop).save("/tmp/vm_c.png")
        except Exception:
            pass
    print("clicked screen(%d,%d) via vncdo(%d,%d)" % (sx, sy, rx, ry))


if __name__ == "__main__":
    main()
