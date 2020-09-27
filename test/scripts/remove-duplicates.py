#!/usr/bin/env python2

import sys
import re

try:
    assert 5 < len(sys.argv) < 8
    assert sys.argv[2] == '-o'
    assert sys.argv[4] == '-l'
    if len(sys.argv) == 7:
        assert sys.argv[6] == '-p'
except AssertionError:
    print("Usage: %s <wordlist> -o <output> -l <max-line-size> [-p]" % sys.argv[0])
    print("Example: %s old.txt -o new.txt -l 14" % sys.argv[0])
    print("Example: %s old.txt -o new.txt -l 40 -p" % sys.argv[0])
    sys.exit(1)

with open(sys.argv[1], 'rb') as f:
    content = f.read()
OLD_CONTENT_LEN = len(content)

output = open(sys.argv[3], 'wb')

MAX_LINE_SIZE = int(sys.argv[5])
FILTER_PRINTABLE = len(sys.argv) == 7


# use re.split(), because str.spllitlines() assumes
# that single "\r" are newline chars too..
lines = re.split(b"\r\n|\n", content)


def line_isprint(line):
    for c in line:
        if not 31 < ord(c) < 127:
            return False
    return True

uniques = []
for index, line in enumerate(lines):
    if not line:
        continue
    if len(line) > MAX_LINE_SIZE:
        continue
    if line.startswith(b"\0"):
        continue
    if line in uniques:
        continue
    if FILTER_PRINTABLE and not line_isprint(line):
        continue
    uniques.append(line)


result = b"\n".join(uniques)
if 0 < len(result) < OLD_CONTENT_LEN:
    result += b'\n'

output.write(result)
output.close()
