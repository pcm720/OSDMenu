# Creates a PSU file from directory contents
import struct
import os
import time
import argparse

PSU_TYPE_DIRECTORY = 0x8427
PSU_TYPE_FILE = 0x8497

class PSUTimestamp:
    def __init__(self, t):
        t = time.gmtime(t)  # Use UTC time
        self.seconds = t.tm_sec
        self.minutes = t.tm_min
        self.hours = t.tm_hour
        self.day = t.tm_mday
        self.month = t.tm_mon
        self.year = t.tm_year

    def to_bytes(self):
        return struct.pack('<BBBBBBH', 0, self.seconds, self.minutes, self.hours, self.day, self.month, self.year)

class PSUHeader:
    def __init__(self, _type, size, name, created, modified):
        self.type = _type
        self.size = size
        self.created = created
        self.modified = modified
        self.name = name.encode('ascii')

    def to_bytes(self):
        return struct.pack('<H2xI8s8x8s32x32s416x', self.type, self.size, self.created.to_bytes(), self.modified.to_bytes(), self.name.ljust(32, b'\0'))

def write_file(out_file, file_info):
    name, data, created, modified = file_info

    header = PSUHeader(PSU_TYPE_FILE, len(data), name, PSUTimestamp(created), PSUTimestamp(modified))
    out_file.write(header.to_bytes())

    # Write file data
    out_file.write(data)

    # Pad to nearest 1024 bytes
    pad_size = (1024 - len(data) % 1024) % 1024
    if pad_size > 0:
        out_file.write(b'\0' * pad_size)

def build_psu(output_path, root_dir, files):
    with open(output_path, 'wb') as out_file:
        now = time.time()

        # Write root directory entry
        root_header = PSUHeader(PSU_TYPE_DIRECTORY, len(files) + 3, root_dir, PSUTimestamp(now), PSUTimestamp(now))
        out_file.write(root_header.to_bytes())

        # Write "." and ".." entries
        out_file.write(PSUHeader(PSU_TYPE_DIRECTORY, 0, ".", PSUTimestamp(now), PSUTimestamp(now)).to_bytes())
        out_file.write(PSUHeader(PSU_TYPE_DIRECTORY, 0, "..", PSUTimestamp(now), PSUTimestamp(now)).to_bytes())

        for file_info in files:
            write_file(out_file, file_info)

def main():
    parser = argparse.ArgumentParser(description='Create a PSU file from directory contents.')
    parser.add_argument('outfile', type=str, help='Path where the PSU file will be saved')
    parser.add_argument('dirname', type=str, help='PSU file directory name')
    parser.add_argument('input', type=str, nargs='+', help='Files or directories to add the files from')

    args = parser.parse_args()

    # Collect all files
    files = []
    for dir in args.input:
        if os.path.isdir(dir):
            for entry in os.scandir(dir):
                if entry.is_file():
                    with open(entry.path, 'rb') as file:
                        data = file.read()

                    stat = entry.stat()
                    files.append((entry.name, data, stat.st_ctime, stat.st_mtime))
        else:
            with open(dir, 'rb') as file:
                data = file.read()

            stat = os.stat(dir)
            files.append((os.path.basename(dir), data, stat.st_ctime, stat.st_mtime))

    # Build PSU
    build_psu(args.outfile, args.dirname, files)

if __name__ == '__main__':
    main()
