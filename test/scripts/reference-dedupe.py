#!/usr/bin/env python3

import sys
import re


def line_isprint(line):
    for c in line:
        if not 31 < c < 127:
            return False
    return True


def dedupe(content, max_line_size, printable=False, lowercase=False, uppercase=False):
    old_content_len = len(content)
    if lowercase:
        content = content.lower()
    elif uppercase:
        content = content.upper()

    # use re.split(), because bytes.splitlines() also splits on single '\r'
    lines = re.split(b"\r\n|\n", content)
    dupes = []
    uniques = []
    for line in lines:
        if not line:
            continue
        if len(line) > max_line_size:
            continue
        if line.startswith(b"\0"):
            continue
        if printable and not line_isprint(line):
            continue
        if line in uniques:
            dupes.append(line)
            continue
        uniques.append(line)

    result = b"\n".join(uniques)
    if 0 < len(result) < old_content_len:
        result += b'\n'
    duplicates = b"\n".join(dupes) + b"\n" if dupes else b""
    return result, duplicates


def main():
    try:
        assert len(sys.argv) >= 8
        assert sys.argv[2] == '-o'
        assert sys.argv[4] == '-D'
        assert sys.argv[6] == '-l'
        if len(sys.argv) > 8:
            for x in sys.argv[8:]:
                assert x in ['-p', '-c', '-C']
            if '-c' in sys.argv[8:]:
                assert '-C' not in sys.argv[8:]
    except AssertionError:
        print("Usage: %s <wordlist> -o <output> -D <dupfile> -l <max-line-size> [-pcC]" % sys.argv[0])
        print("Example: %s old.txt -o new.txt -D dupes.txt -l 14" % sys.argv[0])
        print("Example: %s old.txt -o new.txt -D dupes.txt -l 40 -p" % sys.argv[0])
        print("Example: %s old.txt -o new.txt -D dupes.txt -l 40 -p -C" % sys.argv[0])
        return 1

    with open(sys.argv[1], 'rb') as f:
        content = f.read()
    result, duplicates = dedupe(content, int(sys.argv[7]),
                                '-p' in sys.argv[8:],
                                '-c' in sys.argv[8:],
                                '-C' in sys.argv[8:])
    with open(sys.argv[3], 'wb') as f:
        f.write(result)
    with open(sys.argv[5], 'wb') as f:
        f.write(duplicates)
    return 0


if __name__ == '__main__':
    sys.exit(main())
