import struct
import sys
from pathlib import Path

from elftools.elf.elffile import ELFFile


def symbol(table, name):
    matches = table.get_symbol_by_name(name)
    if not matches:
        raise RuntimeError(f"missing ELF symbol: {name}")
    return matches[0]


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: verify_tls_record_buffer_wrapper_elf.py firmware.elf")
    path = Path(sys.argv[1])
    with path.open("rb") as stream:
        elf = ELFFile(stream)
        table = elf.get_section_by_name(".symtab")
        if table is None:
            raise RuntimeError("ELF has no symbol table")

        wrapper = symbol(table, "__wrap_esp_mbedtls_mem_calloc")
        original = symbol(table, "esp_mbedtls_mem_calloc")
        allocator_pointer = symbol(table, "mbedtls_calloc_func")
        pointer_address = allocator_pointer["st_value"]
        containing = next(
            (section for section in elf.iter_sections()
             if section["sh_addr"] <= pointer_address and
             pointer_address + 4 <= section["sh_addr"] + section["sh_size"]),
            None,
        )
        if containing is None:
            raise RuntimeError("mbedtls_calloc_func outside file-backed sections")

        offset = pointer_address - containing["sh_addr"]
        target = struct.unpack_from("<I", containing.data(), offset)[0]
        if target != wrapper["st_value"]:
            raise RuntimeError(
                f"allocator points to 0x{target:08x}, not wrapper 0x{wrapper['st_value']:08x}"
            )
        print(
            "PASS TLS allocator ELF binding: "
            f"pointer=0x{pointer_address:08x} -> wrapper=0x{wrapper['st_value']:08x}; "
            f"original=0x{original['st_value']:08x} retained"
        )


if __name__ == "__main__":
    main()
