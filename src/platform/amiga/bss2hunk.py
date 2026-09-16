#!/usr/bin/env python3
# bebbo binutils bug: .bss goes out as HUNK_CODE full of zeros


import struct
import sys

HUNK_CODE = 0x3E9
HUNK_DATA = 0x3EA
HUNK_BSS = 0x3EB
HUNK_RELOC32 = 0x3EC
HUNK_RELOC16 = 0x3ED
HUNK_RELOC8 = 0x3EE
HUNK_EXT = 0x3EF
HUNK_SYMBOL = 0x3F0
HUNK_DEBUG = 0x3F1
HUNK_END = 0x3F2
HUNK_RELOC32SHORT = 0x3FC


def read_long(data, pos):
    return struct.unpack_from('>I', data, pos)[0]


def parse_reloc32(data, pos):
    rpos = pos + 4

    while True:
        n = read_long(data, rpos)

        rpos += 4

        if n == 0:
            break

        rpos += 4  # target hunk
        rpos += n * 4  # the offsets

    return rpos


def parse_reloc32short(data, pos):
    n_longs = read_long(data, pos + 4)
    return pos + 8 + n_longs * 4


def parse_symbol(data, pos):
    n_longs = read_long(data, pos + 4)
    return pos + 8 + n_longs * 4


def parse_debug(data, pos):
    n_longs = read_long(data, pos + 4)
    return pos + 8 + n_longs * 4


def skip_sub_hunks(data, pos):
    # can't touch a CODE hunk that has relocs
    has_relocs = False

    while pos < len(data):
        sub_type = read_long(data, pos) & 0x3FFFFFFF

        if sub_type == HUNK_END:
            return pos + 4, has_relocs
        elif sub_type == HUNK_RELOC32:
            has_relocs = True
            pos = parse_reloc32(data, pos)
        elif sub_type == HUNK_RELOC32SHORT:
            has_relocs = True
            pos = parse_reloc32short(data, pos)
        elif sub_type == HUNK_SYMBOL:
            pos = parse_symbol(data, pos)
        elif sub_type == HUNK_DEBUG:
            pos = parse_debug(data, pos)
        else:
            pos += 4

    return pos, has_relocs


def convert(data):
    pos = 0
    htype = read_long(data, pos)

    if htype != 0x3F3:
        raise ValueError(f'Expected HUNK_HEADER, got 0x{htype:03x}')

    pos += 4

    # resident lib names: (length, chars) pairs, zero terminates
    while True:
        n = read_long(data, pos)
        pos += 4

        if n == 0:
            break

        pos += n * 4

    # bebbo sometimes writes last_hunk first, then first_hunk
    val1 = read_long(data, pos)
    val2 = read_long(data, pos + 4)

    pos += 8

    if val1 >= val2:
        last_hunk, first_hunk = val1, val2
    else:
        first_hunk, last_hunk = val1, val2

    num_hunks = last_hunk - first_hunk + 1

    for _ in range(num_hunks):
        pos += 4

    header_end = pos
    output = bytearray(data[:header_end])
    converted = 0

    while pos < len(data):
        htype = read_long(data, pos) & 0x3FFFFFFF

        if htype in (HUNK_CODE, HUNK_DATA):
            size_longs = read_long(data, pos + 4)
            size_bytes = size_longs * 4
            data_start = pos + 8
            data_end = data_start + size_bytes
            is_all_zero = all(b == 0 for b in data[data_start:data_end])

            sub_end, has_relocs = skip_sub_hunks(data, data_end)

            if htype == HUNK_CODE and is_all_zero and not has_relocs:
                # swap type to BSS, keep the size, drop the zero data
                output += struct.pack('>I', HUNK_BSS)
                output += struct.pack('>I', size_longs)
                output += data[data_end:sub_end]
                saved = size_bytes
                converted += 1
                
                print(f'  Converted HUNK_CODE at 0x{pos:06x}: {size_bytes} bytes -> BSS (saved {saved} bytes)')
            else:
                output += data[pos:sub_end]

            pos = sub_end
        elif htype == HUNK_BSS:
            size_longs = read_long(data, pos + 4)
            sub_end, _ = skip_sub_hunks(data, pos + 8)
            output += data[pos:sub_end]
            pos = sub_end
        else:
            output += data[pos:pos + 4]
            pos += 4

    return bytes(output), converted


def main():
    if len(sys.argv) < 2:
        print(f'Usage: {sys.argv[0]} <input> [output]')
        sys.exit(1)

    infile = sys.argv[1]
    outfile = sys.argv[2] if len(sys.argv) > 2 else infile

    with open(infile, 'rb') as f:
        data = f.read()

    orig_size = len(data)
    
    print(f'Input: {infile} ({orig_size} bytes, {orig_size/1024:.1f} KB)')

    output, converted = convert(data)

    new_size = len(output)
    saved = orig_size - new_size

    print(f'Converted {converted} hunk(s)')
    print(f'Output: {outfile} ({new_size} bytes, {new_size/1024:.1f} KB)')
    print(f'Saved: {saved} bytes ({saved/1024:.1f} KB)')

    with open(outfile, 'wb') as f:
        f.write(output)


if __name__ == '__main__':
    main()
