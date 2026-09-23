import os
import struct
import sys

SHT_NULL = 0

DROPPED_NAMES = {
    ".symtab",
    ".strtab",
    ".comment",
    ".note.gnu.build-id",
    ".note.gnu.gold-version",
    ".note.gnu.property",
}


def main(path):
    with open(path, "rb") as fh:
        elf = bytearray(fh.read())
    if elf[:4] != b"\x7fELF" or elf[4] != 2:
        sys.stderr.write(f"elfstrip: unsupported ELF {path}\n")
        return 1

    e_shoff = struct.unpack_from("<Q", elf, 0x28)[0]
    e_shentsize = struct.unpack_from("<H", elf, 0x3A)[0]
    e_shnum = struct.unpack_from("<H", elf, 0x3C)[0]
    e_shstrndx = struct.unpack_from("<H", elf, 0x3E)[0]
    if e_shoff == 0 or e_shentsize < 64:
        return 0

    def sh(idx, off):
        return e_shoff + idx * e_shentsize + off

    shstr_off = struct.unpack_from("<Q", elf, sh(e_shstrndx, 0x18))[0]
    shstr_size = struct.unpack_from("<Q", elf, sh(e_shstrndx, 0x20))[0]
    shstr = elf[shstr_off : shstr_off + shstr_size]

    dropped = set()
    for idx in range(e_shnum):
        name_off = struct.unpack_from("<I", elf, sh(idx, 0x00))[0]
        name = shstr[name_off : shstr.find(b"\x00", name_off)].decode(errors="replace")
        if name in DROPPED_NAMES or name.startswith(".debug"):
            dropped.add(idx)

    if not dropped:
        return 0

    last_end = 0
    for idx in range(e_shnum):
        if idx in dropped:
            continue
        end = struct.unpack_from("<Q", elf, sh(idx, 0x18))[0] + struct.unpack_from("<Q", elf, sh(idx, 0x20))[0]
        last_end = max(last_end, end)

    for idx in dropped:
        elf[sh(idx, 0) : sh(idx, 0) + 64] = b"\x00" * 64

    if last_end < len(elf) and last_end >= struct.unpack_from("<Q", elf, 0x20)[0]:
        del elf[last_end:]

    tmp = path + ".stripped"
    with open(tmp, "wb") as fh:
        fh.write(elf)
    os.replace(tmp, path)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))