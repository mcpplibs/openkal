#!/usr/bin/env python3
"""Probe (mcpp#849, route B): one file of native carriers, no Cosmopolitan libc.

The file IS the Windows carrier (a PE) whose DOS header and stub hold a POSIX
shell prelude; the Linux and macOS carriers follow the PE as overlay bytes. On
Windows the PE runs as it is. On a POSIX system `sh <file>` reads the prelude,
which copies this system's carrier to a cache directory named by the file's
digest (once) and executes it with the arguments.

    make_polyglot.py out <windows.exe> <key>=<carrier> [<key>=<carrier> ...]
    key: linux-x86_64, linux-aarch64, macos-aarch64, macos-x86_64

Temporary: lives on a probe branch and is not merged.
"""
import hashlib
import struct
import sys

PRELUDE = r"""
o=$0; case $o in */*) ;; *) o=$(command -v "$o" 2>/dev/null || echo "./$o");; esac
s=$(uname -s)-$(uname -m)
case $s in
@CASES@  *) echo "polyglot: no carrier for $s in this file" >&2; exit 126;;
esac
d=${XDG_CACHE_HOME:-${HOME:-/tmp}/.cache}/openkal-polyglot/@DIGEST@
t=$d/$k
if [ ! -x "$t" ]; then
  mkdir -p "$d" && chmod 700 "$d" && tail -c +$((f+1)) "$o" | head -c "$n" > "$t.$$" && chmod 755 "$t.$$" && mv -f "$t.$$" "$t" || { echo "polyglot: cannot write $t" >&2; exit 127; }
fi
exec "$t" "$@"
"""

UNAME = {
    "linux-x86_64": "Linux-x86_64",
    "linux-aarch64": "Linux-aarch64|Linux-arm64",
    "macos-aarch64": "Darwin-arm64",
    "macos-x86_64": "Darwin-x86_64",
}


def u16(b, o): return struct.unpack_from("<H", b, o)[0]
def u32(b, o): return struct.unpack_from("<I", b, o)[0]


def main():
    out, win, rest = sys.argv[1], sys.argv[2], sys.argv[3:]
    pe = bytearray(open(win, "rb").read())
    carriers = []
    for item in rest:
        k, p = item.split("=", 1)
        carriers.append((k, open(p, "rb").read()))

    lfanew = u32(pe, 0x3C)
    assert pe[:2] == b"MZ" and pe[lfanew:lfanew + 4] == b"PE\0\0", "not a PE"
    coff = lfanew + 4
    nsec = u16(pe, coff + 2)
    optsz = u16(pe, coff + 16)
    opt = coff + 20
    magic = u16(pe, opt)
    assert magic == 0x20B, "PE32+ only"
    file_align = u32(pe, opt + 36)
    size_headers_at = opt + 60
    size_headers = u32(pe, size_headers_at)
    ddir = opt + 112                       # PE32+ data directories
    sec = opt + optsz
    first_raw = min(u32(pe, sec + 40 * i + 20) for i in range(nsec) if u32(pe, sec + 40 * i + 20))
    first_va = min(u32(pe, sec + 40 * i + 12) for i in range(nsec))

    # The overlay begins where the PE's last raw section (or a certificate) ends.
    pe_end = max(u32(pe, sec + 40 * i + 20) + u32(pe, sec + 40 * i + 16) for i in range(nsec))

    def build(offsets, digest):
        cases = ""
        for (k, data), off in zip(carriers, offsets):
            cases += f"  {UNAME[k]}) k={k}; f={off}; n={len(data)};;\n"
        script = PRELUDE.replace("@CASES@", cases).replace("@DIGEST@", digest).encode()
        head = bytearray(b"MZqFpD='\n")
        head += b" " * (0x3C - len(head))
        head += b"\0\0\0\0"                 # e_lfanew, patched below
        head += b"'\n" + script + b"exit 1\n"
        return head

    # Two passes: the prelude's length decides the shift, the shift decides the offsets.
    offsets, digest = [0] * len(carriers), "0" * 16
    for _ in range(3):
        head = build(offsets, digest)
        new_lfanew = (len(head) + 7) & ~7
        shift = new_lfanew - lfanew
        shift = (shift + file_align - 1) // file_align * file_align   # raw data stays aligned
        new_lfanew = lfanew + shift
        assert new_lfanew >= len(head)
        new_pe_end = pe_end + shift
        offsets, cur = [], new_pe_end
        for _, data in carriers:
            offsets.append(cur)
            cur += len(data)
        digest = hashlib.sha256(b"".join(d for _, d in carriers) + bytes(pe)).hexdigest()[:16]
    head = build(offsets, digest)
    assert size_headers + shift <= first_va, "headers would overlap the first section's address"

    body = bytearray(pe[lfanew:])
    # Offsets within `body` are file offsets minus lfanew.
    def patch32(file_off, delta):
        o = file_off - lfanew
        struct.pack_into("<I", body, o, u32(body, o) + delta)
    patch32(size_headers_at, shift)
    for i in range(nsec):
        s = sec + 40 * i
        if u32(pe, s + 20): patch32(s + 20, shift)    # PointerToRawData
        if u32(pe, s + 24): patch32(s + 24, shift)    # PointerToRelocations
        if u32(pe, s + 28): patch32(s + 28, shift)    # PointerToLinenumbers
    # The debug directory's entries name file offsets (PointerToRawData).
    dbg_rva, dbg_size = u32(pe, ddir + 8 * 6), u32(pe, ddir + 8 * 6 + 4)
    if dbg_rva and dbg_size:
        for i in range(nsec):
            s = sec + 40 * i
            va, vsz, raw = u32(pe, s + 12), max(u32(pe, s + 8), u32(pe, s + 16)), u32(pe, s + 20)
            if va <= dbg_rva < va + vsz:
                at = raw + (dbg_rva - va)
                for e in range(dbg_size // 28):
                    ptr = at + 28 * e + 24
                    if u32(pe, ptr): patch32(ptr, shift)
    assert u32(pe, ddir + 8 * 4) == 0, "an already signed PE"

    outb = bytearray(head) + b"\n" * (new_lfanew - len(head)) + body[: pe_end - lfanew]
    struct.pack_into("<I", outb, 0x3C, new_lfanew)
    assert b"'" not in outb[9:0x3C + 4], "a quote inside the quoted header"
    # Everything between the header and the PE header is in the prelude, after
    # `exit 1`; the PE header and sections follow.
    for _, data in carriers:
        outb += data
    assert len(outb) == offsets[-1] + len(carriers[-1][1]) if carriers else True
    open(out, "wb").write(outb)
    print(f"{out}: {len(outb)} bytes; PE header moved {lfanew:#x} -> {new_lfanew:#x}; carriers at "
          + ", ".join(f"{k}@{o}+{len(d)}" for (k, d), o in zip(carriers, offsets)))


if __name__ == "__main__":
    main()
