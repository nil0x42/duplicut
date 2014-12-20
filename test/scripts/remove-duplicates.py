#!/usr/bin/env python

import sys
import re

if len(sys.argv) != 4:
    print("Usage: %s <wordlist> <max-line-size> <output>" % sys.argv[0])
    sys.exit(1)


wordlist = open(sys.argv[1], 'r')
content = wordlist.read()
wordlist.close()
if not content:
    sys.exit(0)


MAX_LINE_SIZE = int(sys.argv[2])
ADD_ENDING_NEWLINE = True if content.endswith("\n") else False


output = open(sys.argv[3], 'w')


# use re.split(), because str.spllitlines() assumes
# that single "\r" are newline chars too..
lines = re.split("\r\n|\n", content)


uniques = []
for index, line in enumerate(lines):
    if not line:
        continue
    if len(line) > MAX_LINE_SIZE:
        continue
    if line.startswith("\0"):
        continue
    if line in uniques:
        continue
    uniques.append(line)


result = "\n".join(uniques)

if ADD_ENDING_NEWLINE:
    result += "\n"

output.write(result)
output.close()
