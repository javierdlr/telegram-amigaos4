#!/usr/bin/env python3
"""Produce a flashless Workbench launcher icon from an existing .info.

The stock TelegramAmiga.info is a WBPROJECT icon whose DefaultTool is C:IconX,
so a double-click runs the IconX launcher script -> a console window flashes.
This rewrites just two fields so a double-click launches the binary directly:

  - do_DefaultTool string  C:IconX            -> <tool>   (default: TelegramAmiga)
  - do_StackSize           (whatever it was)  -> <stack>  (default: 1048576)

Everything else (the OldIcon imagery, the ToolTypes, and the trailing GlowIcon
IFF) is preserved byte-for-byte. The binary detects the argc==0 Workbench start
and runs --gui-live itself, so the IconX script is no longer needed for the GUI.

The .info OldIcon layout we touch:
  [0:78]      DiskObject header (do_StackSize is the last LONG, at offset 74)
  [78:D]      gadget imagery (opaque, kept verbatim)
  [D:..]      do_DefaultTool : LONG len + chars (len counts the NUL)
  [..:..]     do_ToolTypes   : LONG (n+1)*4, then n x (LONG len + chars)
  [..:end]    trailing (GlowIcon/NewIcon IFF), kept verbatim

We do not parse the imagery: we locate the DefaultTool record by its length
prefix and parse only the deterministic string section forward. A round-trip
with no edits must reproduce the input exactly, which we assert before writing.
"""
import struct
import sys


def parse(data):
    if data[0:2] != b"\xe3\x10":
        raise ValueError("not a .info file (bad magic)")
    off = 48
    do_type = data[off]
    (default_tool, tool_types, cx, cy, drawer, toolwin, stack) = struct.unpack(
        ">IIiiIII", data[off + 2:off + 2 + 28])
    # Find the DefaultTool record: its length LONG sits immediately before the
    # string. Search for <LONG len><len bytes ending in NUL> right after the
    # imagery. We anchor on the known stock value C:IconX, but verify via the
    # length prefix so the parse is structural, not a blind string replace.
    dtool_pos = None
    if default_tool:
        idx = data.find(b"C:IconX\x00", 78)
        if idx < 0:
            raise ValueError("DefaultTool C:IconX not found")
        ln = struct.unpack(">I", data[idx - 4:idx])[0]
        if ln != len(b"C:IconX\x00"):
            raise ValueError("DefaultTool length prefix mismatch")
        dtool_pos = idx - 4
    if dtool_pos is None:
        raise ValueError("icon has no DefaultTool to rewrite")
    imagery = data[78:dtool_pos]
    p = dtool_pos
    dt_len = struct.unpack(">I", data[p:p + 4])[0]
    p += 4
    dt_str = data[p:p + dt_len]
    p += dt_len
    # ToolTypes block (kept verbatim).
    tt_start = p
    if tool_types:
        cnt = struct.unpack(">I", data[p:p + 4])[0]
        p += 4
        n = cnt // 4 - 1
        for _ in range(n):
            sl = struct.unpack(">I", data[p:p + 4])[0]
            p += 4 + sl
    tooltypes = data[tt_start:p]
    trailing = data[p:]
    return {
        "header": bytearray(data[0:78]), "imagery": imagery,
        "dt_str": dt_str, "tooltypes": tooltypes, "trailing": trailing,
        "do_type": do_type, "stack": stack,
    }


def serialize(s, default_tool=None, stack=None):
    header = bytearray(s["header"])
    if stack is not None:
        header[74:78] = struct.pack(">I", stack)
    dt = s["dt_str"] if default_tool is None else (default_tool.encode() + b"\x00")
    out = bytes(header) + s["imagery"]
    out += struct.pack(">I", len(dt)) + dt
    out += s["tooltypes"] + s["trailing"]
    return out


def main():
    if len(sys.argv) < 3:
        sys.stderr.write(
            "usage: make_gui_icon.py IN.info OUT.info [tool] [stack]\n")
        return 2
    src, dst = sys.argv[1], sys.argv[2]
    tool = sys.argv[3] if len(sys.argv) > 3 else "TelegramAmiga"
    stack = int(sys.argv[4]) if len(sys.argv) > 4 else 1048576
    data = open(src, "rb").read()
    s = parse(data)
    # Round-trip guard: reserialize unchanged must equal the input exactly.
    rt = serialize(s)
    if rt != data:
        sys.stderr.write("FATAL: round-trip mismatch (%d vs %d bytes) -- "
                         "parser does not understand this icon; refusing.\n"
                         % (len(rt), len(data)))
        return 1
    out = serialize(s, default_tool=tool, stack=stack)
    open(dst, "wb").write(out)
    print("wrote %s: do_Type=%d DefaultTool=%r->%r stack=%d->%d (%d bytes)"
          % (dst, s["do_type"], s["dt_str"].rstrip(b"\x00").decode(),
             tool, s["stack"], stack, len(out)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
