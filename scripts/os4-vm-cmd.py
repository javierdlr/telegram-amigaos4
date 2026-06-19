#!/usr/bin/env python3
"""Type a command line into the focused MorphOS shell and press Enter, using the
QEMU monitor 'sendkey' for EVERY keystroke (vncdotool drops shift on this guest,
turning ':' into ';' and '>' into '.', so we avoid it for typing).

Usage:
  morphos-vm-cmd.py "command line"        # type + Enter + screendump
  morphos-vm-cmd.py --noenter "text"      # type only
  morphos-vm-cmd.py --enteronly           # just Enter
  morphos-vm-cmd.py --clear               # Ctrl-X (clear current input line)
  morphos-vm-cmd.py --wait 6 "command"    # wait 6s before the shot
  morphos-vm-cmd.py --noshot "command"    # skip the screendump
Screendump (when taken) -> /tmp/vm.png ; a cropped shell strip -> /tmp/vm_c.png
"""
import socket, subprocess, sys, time

MON = ("/Volumes/EXT/Macchine Virtuali/Amiga/emu/telegram-amiga/"
       "os4/qemu-monitor-2223.sock")

# char -> QEMU sendkey qcode (US keyboard)
M = {}
for c in "abcdefghijklmnopqrstuvwxyz0123456789":
    M[c] = c
for c in "abcdefghijklmnopqrstuvwxyz":
    M[c.upper()] = "shift-" + c
M.update({
    ' ': 'spc', '.': 'dot', ',': 'comma', '/': 'slash', '\\': 'backslash',
    '-': 'minus', '=': 'equal', ';': 'semicolon', "'": 'apostrophe',
    '`': 'grave_accent', '[': 'bracket_left', ']': 'bracket_right',
    ':': 'shift-semicolon', '>': 'shift-dot', '<': 'shift-comma',
    '?': 'shift-slash', '_': 'shift-minus', '+': 'shift-equal',
    '"': 'shift-apostrophe', '!': 'shift-1', '@': 'shift-2', '#': 'shift-3',
    '$': 'shift-4', '%': 'shift-5', '^': 'shift-6', '&': 'shift-7',
    '*': 'shift-8', '(': 'shift-9', ')': 'shift-0',
    '{': 'shift-bracket_left', '}': 'shift-bracket_right',
    '|': 'shift-backslash', '~': 'shift-grave_accent',
})


def mon_send(lines, settle=0.4):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(15); s.connect(MON); time.sleep(0.2)
    try: s.recv(65536)
    except Exception: pass
    for ln in lines:
        s.sendall((ln + "\n").encode())
        time.sleep(0.03)
    time.sleep(settle)
    out = b""
    try:
        while True:
            c = s.recv(65536)
            if not c: break
            out += c
            if len(c) < 65536: break
    except Exception: pass
    s.close(); return out.decode(errors="replace")


# Italian keymap overrides (the INSTALLED MorphOS uses the Italian keymap):
# only the special chars differ from US; letters/digits/space are the same.
M_IT = dict(M)
M_IT.update({
    ':': 'shift-dot', '/': 'shift-7', '-': 'slash', '_': 'shift-slash',
    ';': 'shift-comma', "'": 'minus', '?': 'shift-slash', '=': 'shift-0',
    '(': 'shift-8', ')': 'shift-9', '&': 'shift-6', '"': 'shift-2',
    '.': 'dot', ',': 'comma',
})


def main():
    a = sys.argv[1:]
    do_enter = True; do_shot = True; wait = 2.0; clear = False; itmap = False
    while a and a[0].startswith("--"):
        f = a.pop(0)
        if f == "--noenter": do_enter = False
        elif f == "--enteronly": pass
        elif f == "--noshot": do_shot = False
        elif f == "--clear": clear = True
        elif f == "--it": itmap = True
        elif f == "--wait": wait = float(a.pop(0))
        else: break
    text = a[0] if a else ""
    table = M_IT if itmap else M

    keys = []
    if clear:
        keys.append("sendkey ctrl-x")
    for ch in text:
        qc = table.get(ch)
        if qc is None:
            sys.stderr.write("no mapping for %r\n" % ch); continue
        keys.append("sendkey " + qc)
    if do_enter:
        keys.append("sendkey ret")
    if keys:
        mon_send(keys)

    if do_shot:
        time.sleep(wait)
        mon_send(['screendump "/tmp/vm.ppm"'])
        subprocess.run(["sips", "-s", "format", "png", "/tmp/vm.ppm",
                        "--out", "/tmp/vm.png"], capture_output=True)
        try:
            from PIL import Image
            Image.open("/tmp/vm.png").crop((0, 60, 920, 200)).resize(
                (1380, 210)).save("/tmp/vm_c.png")
        except Exception:
            pass
        print("shot /tmp/vm.png (crop /tmp/vm_c.png)")


if __name__ == "__main__":
    main()
