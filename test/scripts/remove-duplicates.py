#!/usr/bin/env python

import sys
import re

if len(sys.argv) != 5:
    print("Usage: %s <wordlist> <max-line-size> <filter_printable> <output>" % sys.argv[0])
    sys.exit(1)


content = open(sys.argv[1], 'rb').read()
OLD_CONTENT_LEN = len(content)

MAX_LINE_SIZE = int(sys.argv[2])
FILTER_PRINTABLE = int(sys.argv[3])

output = open(sys.argv[4], 'wb')


# use re.split(), because str.spllitlines() assumes
# that single "\r" are newline chars too..
lines = re.split(b"\r\n|\n", content)


def line_isprint(line):
    for c in line:
        if not 31 < c < 127:
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
