#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
#
# PS2 ROM to OSDR converter
# Extracts ROM 2.20 OSDSYS and its resources from PS2 ROM image and packs them into OSDR format binary
# Based on https://github.com/ps2repack/scosdsys by Julian Uy

"""
The OSDR format is a container for PlayStation 2 OSDSYS resources.

Header:
    Magic: "OSDR" (4 bytes, little-endian)

Resource entries (repeated until EOF mark):
    Each resource entry consists of:
    - type: uint32 (resource type identifier)
    - compressedSize: uint32 (size of compressed data in bytes)
    - uncompressedSize: uint32 (size of decompressed data in bytes)
    - name: 16 bytes (ASCII resource name, non-terminated)
    - compressedData: compressedSize bytes (zlib-compressed resource data)

Resource types:
    OSDR_TYPE_IOPRP = 0        - IOPRP image
    OSDR_TYPE_IRX = 1          - IRX module
    OSDR_TYPE_OSDSYS = 2       - Uncompressed OSDSYS code, to be loaded at 0x200000
    OSDR_TYPE_OSD_RESOURCE = 3 - Other OSD resources
    OSDR_TYPE_EOF = 0xFFFFFFFF - EOF mark

Resource order:
    - IOPRP
    - XDEV9
    - XDEV9SERV
    - OSDSYS
    - Resources

EOF mark:
    When the resource type is OSDR_TYPE_EOF, the remaining fields are zeroed and no compressed data follows.

Layout:
    [OSDR magic]
    [resource entry 1]
    [resource entry 2]
    ...
    [EOF mark]

Usage:
    python3 rom_to_osdr.py <input_rom_file> <output_osdr_file>
"""

import io
import os
import struct
import sys
import zlib


def read_items(in_bytes=None, in_items=None, in_dic=None, in_byteorder="little"):
    total_offset = 0
    for x in in_items:
        item_name = x[0]
        item_size = x[1]
        item_element_size = x[2] if len(x) > 2 else None
        if in_bytes != None:
            item_bytes = in_bytes[total_offset : total_offset + item_size]
            if len(item_bytes) != item_size:
                break
            item_data = item_bytes
            if item_element_size != None:
                item_arr = []
                for i in range(item_size // item_element_size):
                    item_arr.append(
                        int.from_bytes(
                            item_bytes[
                                i * item_element_size : (i + 1) * item_element_size
                            ],
                            byteorder=in_byteorder,
                        )
                    )
                if len(item_arr) == 1:
                    item_data = item_arr[0]
                else:
                    item_data = item_arr
            if in_dic != None:
                in_dic[item_name] = item_data
        total_offset += item_size
    return total_offset


dentry_items = [
    ["name", 10],
    ["ext_info_entry_size", 2, 2],
    ["size", 4, 4],
]

extinfo_items = [
    ["value", 2, 2],
    ["ext_length", 1, 1],
    ["type", 1, 1],
]


def get_romdir_offset(in_bytes, max_search_size=0x8000):
    entry_size = read_items(None, dentry_items, None)
    max_search_entries = max_search_size // entry_size
    entry_dic = {}
    for i in range(max_search_entries):
        read_items(
            in_bytes[entry_size * i : entry_size * (i + 1)],
            dentry_items,
            entry_dic,
            "little",
        )
        if entry_dic["name"] == b"RESET\x00\x00\x00\x00\x00" and (
            ((entry_dic["size"] + 15) & (15 ^ 0xFFFFFFFF)) == (entry_size * i)
        ):
            return entry_size * i
        read_items(
            in_bytes[entry_size * i : entry_size * (i + 1)],
            dentry_items,
            entry_dic,
            "big",
        )
        if entry_dic["name"] == b"RESETB\x00\x00\x00\x00" and (
            (entry_dic["size"] + 15) & (15 ^ 0xFFFFFFFF)
        ) == (entry_size * i):
            return entry_size * i
    return None


def get_romdir_entries(in_bytes, romdir_offset):
    entry_size = read_items(None, dentry_items, None)
    entries = []
    item_endian = "little"
    for i in range(2):
        entry_dic = {}
        read_items(
            in_bytes[
                romdir_offset + (entry_size * i) : romdir_offset
                + (entry_size * (i + 1))
            ],
            dentry_items,
            entry_dic,
            item_endian,
        )
        if i == 0 and entry_dic["name"] == b"RESETB\x00\x00\x00\x00":
            item_endian = "big"
            read_items(
                in_bytes[
                    romdir_offset + (entry_size * i) : romdir_offset
                    + (entry_size * (i + 1))
                ],
                dentry_items,
                entry_dic,
                item_endian,
            )
        entries.append(entry_dic)
    romdir_count = (entries[1]["size"] // entry_size) - 1
    for i in range(2, romdir_count, 1):
        entry_dic = {}
        read_items(
            in_bytes[
                romdir_offset + (entry_size * i) : romdir_offset
                + (entry_size * (i + 1))
            ],
            dentry_items,
            entry_dic,
            item_endian,
        )
        entries.append(entry_dic)
    return entries


EXTINFO_FIELD_TYPE_DATE = 1
EXTINFO_FIELD_TYPE_VERSION = 2
EXTINFO_FIELD_TYPE_COMMENT = 3
EXTINFO_FIELD_TYPE_FIXED = 0x7F


def convert_from_base_16(value):
    result = value & 0xF
    result += (value & 0xF0) // 0x10 * 10
    result += (value & 0xF00) // 0x100 * 100
    result += (value & 0xF000) // 0x1000 * 1000
    return result


def convert_to_base_16(value):
    result = value
    result += value // 10 * 0x6
    result += value // 100 * 0x60
    result += value // 1000 * 0x600
    return result


def get_extinfo_entries(in_bytes, in_byteorder):
    entries = []
    entry_size = read_items(None, extinfo_items, None)
    extinfo_offset = 0
    while extinfo_offset < len(in_bytes):
        entry_dic = {}
        entry_dic["offset"] = extinfo_offset
        read_items(
            in_bytes[extinfo_offset : extinfo_offset + entry_size],
            extinfo_items,
            entry_dic,
            in_byteorder,
        )
        extinfo_offset += entry_size
        entry_dic["ext"] = in_bytes[
            extinfo_offset : extinfo_offset + entry_dic["ext_length"]
        ]
        extinfo_offset += entry_dic["ext_length"]
        if entry_dic["type"] == EXTINFO_FIELD_TYPE_DATE:
            entry_dic["date"] = "%04d/%02d/%02d" % (
                convert_from_base_16(
                    int.from_bytes(entry_dic["ext"][2:4], byteorder="little")
                ),
                convert_from_base_16(
                    int.from_bytes(entry_dic["ext"][1:2], byteorder="little")
                ),
                convert_from_base_16(
                    int.from_bytes(entry_dic["ext"][0:1], byteorder="little")
                ),
            )
        if entry_dic["type"] == EXTINFO_FIELD_TYPE_VERSION:
            entry_dic["version"] = entry_dic["value"]
        if entry_dic["type"] == EXTINFO_FIELD_TYPE_COMMENT:
            entry_dic["comment"] = entry_dic["ext"].rstrip(b"\x00")
        if entry_dic["type"] == EXTINFO_FIELD_TYPE_FIXED:
            entry_dic["need_fixed_offset"] = True
        entries.append(entry_dic)
    return entries


def read_romdir(in_bytes, write_file_callback):
    romdir_offset = get_romdir_offset(in_bytes)
    if romdir_offset == None:
        raise Exception("invalid")
    file_metadata = {}
    file_metadata["file_ext"] = "img"
    write_file_callback(None, None, file_metadata)
    entries = get_romdir_entries(in_bytes, romdir_offset)
    total_offset = romdir_offset
    extinfo_entries = None
    cur_endian = "little"
    for entry in entries:
        name_trimmed = entry["name"].rstrip(b"\x00")
        start_offset = total_offset
        if name_trimmed == b"RESET" or name_trimmed == b"RESETB":
            start_offset = romdir_offset - entry["size"]
            cur_endian = "big" if (name_trimmed == b"RESETB") else "little"
        else:
            total_offset += (entry["size"] + 15) & (15 ^ 0xFFFFFFFF)
        entry["offset"] = start_offset
        if name_trimmed == b"EXTINFO":
            file_bytes = in_bytes[start_offset : start_offset + entry["size"]]
            extinfo_entries = get_extinfo_entries(file_bytes, cur_endian)
    total_extinfo_offset = 0
    for entry in entries:
        name_trimmed = entry["name"].rstrip(b"\x00")
        extinfo_offset_start = total_extinfo_offset
        extinfo_offset_end = extinfo_offset_start + entry["ext_info_entry_size"]
        total_extinfo_offset += entry["ext_info_entry_size"]
        cur_metadata = {}
        if extinfo_entries != None:
            for extinfo_entry in extinfo_entries:
                extinfo_entry_offset = extinfo_entry["offset"]
                if (
                    extinfo_entry_offset >= extinfo_offset_start
                    and extinfo_entry_offset < extinfo_offset_end
                ):
                    if extinfo_entry["type"] == EXTINFO_FIELD_TYPE_DATE:
                        cur_metadata["date"] = extinfo_entry["date"]
                        break
            for extinfo_entry in extinfo_entries:
                extinfo_entry_offset = extinfo_entry["offset"]
                if (
                    extinfo_entry_offset >= extinfo_offset_start
                    and extinfo_entry_offset < extinfo_offset_end
                ):
                    if extinfo_entry["type"] == EXTINFO_FIELD_TYPE_FIXED:
                        cur_metadata["fixed_offset"] = entry["offset"]
                        break
        file_bytes = in_bytes[entry["offset"] : entry["offset"] + entry["size"]]
        write_file_callback(name_trimmed, file_bytes, cur_metadata)


def identify_romdir(in_bytes, in_bytes_len):
    return (
        (
            in_bytes[0:10] == b"RESET\x00\x00\x00\x00\x00"
            and (in_bytes_len > int.from_bytes(in_bytes[28:32], byteorder="little"))
        )
        or (
            in_bytes[0:10] == b"RESETB\x00\x00\x00\x00"
            and (in_bytes_len > int.from_bytes(in_bytes[28:32], byteorder="big"))
        )
        or (
            in_bytes[0x108:0x128] == b"Sony Computer Entertainment Inc."
            and len(in_bytes) >= 0x8000
        )
    )


def read_romdir_dic(src):
    dic = {}

    def write_file_cb(name, file_bytes, file_metadata):
        if file_bytes != None:
            dic[name.decode("ASCII")] = file_bytes

    read_romdir(src, write_file_cb)
    return dic


def decompress_osdsys(src, dst):
    run = 0
    src_offset = 0
    dst_offset = 0
    state_length = 0
    state_block_desc = 0
    state_n = 0
    state_shift = 0
    state_mask = 0

    state_length = int.from_bytes(src[src_offset : src_offset + 4], byteorder="little")
    src_offset += 4

    def safe_read(src, src_offset):
        if src_offset >= len(src) or src_offset < 0:
            return 0
        return src[src_offset]

    def safe_write(dst, dst_offset, val):
        if dst_offset >= len(dst) or dst_offset < 0:
            return
        dst[dst_offset] = val

    while dst_offset <= state_length:
        if run == 0:
            run = 30
            state_block_desc = 0
            for i in range(4):
                state_block_desc <<= 8
                state_block_desc |= safe_read(src, src_offset)
                src_offset += 1
            state_n = state_block_desc & 3
            state_shift = 14 - state_n
            state_mask = 0x3FFF >> state_n
        if (state_block_desc & (1 << (run + 1))) == 0:
            safe_write(dst, dst_offset, safe_read(src, src_offset))
            dst_offset += 1
            src_offset += 1
        else:
            h = safe_read(src, src_offset) << 8
            src_offset += 1
            h |= safe_read(src, src_offset)
            src_offset += 1
            copy_offset = dst_offset - ((h & state_mask) + 1)
            m = 2 + (h >> state_shift)
            for i in range(m + 1):
                safe_write(dst, dst_offset, safe_read(dst, copy_offset))
                dst_offset += 1
                copy_offset += 1
        run -= 1


def decompress_osdsys_res_outer(src):
    outdata_len = int.from_bytes(src[0:4], byteorder="little")
    if outdata_len > len(src) * 16:
        raise Exception("Invalid output length")
    outdata = bytearray(outdata_len)
    decompress_osdsys(src, outdata)
    return bytes(outdata)


def decompress_osdsys_outer(src):
    return decompress_osdsys_res_outer(src[3584:])


def read_null_ending_string(f):
    import functools
    import itertools

    toeof = iter(functools.partial(f.read, 1), b"")
    return sys.intern(
        (b"".join(itertools.takewhile(b"\x00".__ne__, toeof))).decode("ASCII")
    )


def write_ioprp(arr, f):
    items = [dic["name"] for dic in arr]
    if "RESET" in items or "ROMDIR" in items or "" in items:
        raise Exception("currently unhandled")
    if len(list(set(sorted(items)))) != len(items):
        raise Exception("currently unhandled")

    def write_romdir_entry(f, name, extinfo_size, data_size):
        print(f"\t{name}")
        name_e = name.encode("ASCII")[:10]
        extinfo_size_e = extinfo_size.to_bytes(length=2, byteorder="little")
        data_size_e = data_size.to_bytes(length=4, byteorder="little")
        f.write(name_e + (b"\x00" * (10 - len(name_e))))
        f.write(extinfo_size_e)
        f.write(data_size_e)

    write_romdir_entry(f, "RESET", 0, 0)
    write_romdir_entry(f, "ROMDIR", 0, 16 * (len(arr) + 3))
    for dic in arr:
        write_romdir_entry(f, dic["name"], 0, len(dic["data"]))
    write_romdir_entry(f, "", 0, 0)
    for dic in arr:
        f.write(dic["data"])
        f.write(
            b"\x00" * (((((len(dic["data"]) - 1) >> 4) + 1) << 4) - len(dic["data"]))
        )


# Attempts to find resource table in OSDSYS by checking if addr contains a pointer to FONTM string
def probe_resource_table(f, addr):
    f.seek(addr)
    nameaddr = int.from_bytes(f.read(4), byteorder="little")
    try:
        f.seek(nameaddr - 0x200000)
        name = read_null_ending_string(f)
        f.seek(addr)
        if name == "FONTM":
            return addr

    except (OSError, ValueError):
        return 0
    return 0


# OSDR resource types
OSDR_TYPE_IOPRP = 0
OSDR_TYPE_IRX = 1
OSDR_TYPE_OSDSYS = 2
OSDR_TYPE_OSD_RESOURCE = 3
OSDR_TYPE_EOF = 0xFFFFFFFF

# IRX files to include in the OSDR binary
IRX_FILES = ["XDEV9", "XDEV9SERV"]


def write_osdr_resource(f, name, data, res_type):
    print(f"\t{name}")
    if len(data) == 0:
        f.write(
            struct.pack(
                "<III",
                res_type,
                0,
                0,
            )
        )
        f.write(f"{name:{'\x00'}<16}".encode("ASCII"))
        return

    compressed = zlib.compress(data)

    # Pad compressed data to 16-byte boundary
    padding = (16 - (len(compressed) % 16)) % 16
    if padding > 0:
        compressed += b"\x00" * padding

    f.write(
        struct.pack(
            "<III",
            res_type,
            len(compressed),
            len(data),
        )
    )
    f.write(f"{name:{'\x00'}<16}".encode("ASCII"))
    f.write(compressed)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <infile> <outfile>")
        exit(1)
    infile_name = sys.argv[1]
    outfile_name = sys.argv[2]
    if not os.path.isfile(infile_name):
        print(f"Input file not found: {infile_name}")
        exit(1)
    with open(infile_name, "rb") as f:
        infile_bytes = f.read()

    # Parse ROMDIR
    filedic = {}
    romdic = read_romdir_dic(infile_bytes)
    for fn in romdic:
        filedic[fn] = romdic[fn]

    # Check ROM version
    romver = filedic["ROMVER"].decode("ASCII").split("\n")[0]
    if int(romver[:4]) != 220:
        print(
            f"Only ROM versions 2.20 (CEX/DEX) are supported, the input file is ROM {romver}"
        )
        exit(1)
    print(f"Using ROM {romver} as a base")

    # Decompress OSDSYS
    decromdic = {}
    decromdic["OSDSYS"] = decompress_osdsys_outer(filedic["OSDSYS"])
    resarr = []

    with io.BytesIO(decromdic["OSDSYS"]) as f:
        restable = probe_resource_table(f, 0x7BC78)
        if restable == 0:
            restable = probe_resource_table(f, 0x7D500)
        if restable == 0:
            print("Failed to find resource data")
            exit(1)

        print(f"Found OSDSYS resource table at offset {restable}")

        for i in range(83):
            dic = {}
            dic["name_addr"] = int.from_bytes(f.read(4), byteorder="little")
            dic["dest_addr"] = int.from_bytes(f.read(4), byteorder="little")
            dic["size_bytes"] = int.from_bytes(f.read(4), byteorder="little")
            dic["flags"] = int.from_bytes(f.read(4), byteorder="little")
            resarr.append(dic)
        for dic in resarr:
            f.seek(dic["name_addr"] - 0x200000)
            dic["name"] = read_null_ending_string(f)
    for dic in resarr:
        if dic["flags"] == 2:
            resdic = read_romdir_dic(filedic[dic["name"]])
            for fn in resdic:
                if fn not in filedic:
                    filedic[fn] = resdic[fn]
    for dic in resarr:
        if dic["flags"] != 2:
            decromdic[dic["name"]] = decompress_osdsys_res_outer(filedic[dic["name"]])
            dic["size_bytes"] = len(decromdic[dic["name"]])

    ioprp_override = []
    ioprp_add = []
    main_iopbtconf = filedic["IOPBTCONF"].decode("ASCII").split("\n")
    osdcnf_iopbtconf = read_romdir_dic(filedic["OSDCNF"])["IOPBTCONF"]
    osdcnf_iopbtconf_split = osdcnf_iopbtconf.decode("ASCII").split("\n")
    for entry in osdcnf_iopbtconf_split:
        if entry not in main_iopbtconf:
            if entry[:1] == "X" and entry[1:] in main_iopbtconf:
                ioprp_override.append(entry)
            else:
                ioprp_add.append(entry)
    osdrp_items = []
    osdrp_item_dic = {}
    osdrp_iopbtconf_items = []
    osdrp_iopbtconf_items.append("!include IOPBTCONF")
    for entry in ioprp_add:
        osdrp_iopbtconf_items.append(entry)
    osdrp_iopbtconf_items.append("")
    osdrp_item_dic["name"] = "IOPBTCONF"
    osdrp_item_dic["data"] = osdcnf_iopbtconf
    osdrp_items.append(osdrp_item_dic)
    for entry in ioprp_override:
        osdrp_item_dic = {}
        osdrp_item_dic["name"] = entry
        osdrp_item_dic["data"] = filedic[entry]
        osdrp_items.append(osdrp_item_dic)
    for entry in ioprp_add:
        osdrp_item_dic = {}
        osdrp_item_dic["name"] = entry
        osdrp_item_dic["data"] = filedic[entry]
        osdrp_items.append(osdrp_item_dic)

    print("Building IOPRP image")
    with io.BytesIO() as wf2:
        write_ioprp(osdrp_items, wf2)
        ioprp_data = wf2.getvalue()

    irx_files_data = {}
    for irx_name in IRX_FILES:
        irx_files_data[irx_name] = filedic[irx_name]

    print("Building OSDR file")
    with io.BytesIO() as of:
        # Write magic
        of.write(b"OSDR")

        # IOPRP
        write_osdr_resource(of, "IOPRP", ioprp_data, OSDR_TYPE_IOPRP)

        # IRX files
        for irx_name in IRX_FILES:
            write_osdr_resource(of, irx_name, irx_files_data[irx_name], OSDR_TYPE_IRX)

        # OSDSYS
        write_osdr_resource(of, "OSDSYS", decromdic["OSDSYS"], OSDR_TYPE_OSDSYS)

        # Other OSD resources
        print("\n\tResources:")
        for dic in resarr:
            name = dic.get("name", "")
            if not name:
                continue
            if dic["flags"] == 2:
                write_osdr_resource(of, name, b"", OSDR_TYPE_OSD_RESOURCE)
                continue
            if name in ("OSDSYS",) + tuple(IRX_FILES):
                continue
            if dic["size_bytes"] == 0:
                continue
            write_osdr_resource(of, name, decromdic[name], OSDR_TYPE_OSD_RESOURCE)

        # EOF marker
        of.write(struct.pack("<IIII", OSDR_TYPE_EOF, 0, 0, 0))

        # Write to file
        of.seek(0)
        with open(outfile_name, "wb") as out:
            out.write(of.read())

    print(f"\nOSDR file written to {outfile_name}")
