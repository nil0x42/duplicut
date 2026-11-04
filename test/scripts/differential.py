#!/usr/bin/env python3

import runpy
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path


dedupe = runpy.run_path(str(Path(__file__).with_name('reference-dedupe.py')))['dedupe']


def test_case(wordlist, args, expected, output, dupfile):
    output.unlink(missing_ok=True)
    dupfile.unlink(missing_ok=True)
    command = ['./duplicut', '-o', str(output), '-D', str(dupfile), '-m', '64K', *args]
    stderr = b''
    try:
        with wordlist.open('rb') as source:
            result = subprocess.run(command, stdin=source, stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE, timeout=5)
        stderr = result.stderr
        if result.returncode != 0:
            reason = 'exit status %d' % result.returncode
        elif not output.is_file() or not dupfile.is_file():
            reason = 'missing output file'
        elif output.read_bytes() != expected[0]:
            reason = 'different unique lines'
        elif sorted(dupfile.read_bytes().split(b'\n')) != sorted(expected[1].split(b'\n')):
            reason = 'different duplicate lines'
        else:
            return True
    except subprocess.TimeoutExpired as error:
        reason = 'timeout (5 seconds)'
        stderr = error.stderr or b''
    except OSError as error:
        reason = str(error)

    reproduce = ['./duplicut', '-o', 'differential-unique-duplicut.out',
                 '-D', 'differential-duplicates-duplicut.out', '-m', '64K', *args]
    print('[!] %s: %s < %s' % (reason, shlex.join(reproduce), shlex.quote(str(wordlist))),
          file=sys.stderr)
    if stderr:
        print(stderr.decode('utf-8', errors='backslashreplace'), end='', file=sys.stderr)
    for kind, path, content in zip(('unique', 'duplicates'), (output, dupfile), expected):
        reference = Path('differential-%s-reference.out' % kind)
        actual = Path('differential-%s-duplicut.out' % kind)
        reference.write_bytes(content)
        actual.unlink(missing_ok=True)
        if path.is_file():
            actual.write_bytes(path.read_bytes())
        print('[!] Outputs: %s, %s' % (reference, actual), file=sys.stderr)
    return False


def main():
    wordlists = sorted(Path('test/wordlists').glob('*.txt'))
    if not wordlists:
        print('[!] No test wordlists found', file=sys.stderr)
        return 1

    sizes = (1, 5, 14, 15, 40, 65, 128, 2000, 4095)
    options = ((), ('-p',), ('-c',), ('-p', '-c'), ('-C',), ('-p', '-C'))
    cases = 0
    with tempfile.TemporaryDirectory(prefix='duplicut-tests-') as directory:
        output = Path(directory) / 'output'
        dupfile = Path(directory) / 'duplicates'
        for wordlist in wordlists:
            content = wordlist.read_bytes()
            for size in sizes:
                for flags in options:
                    args = ['-l', str(size), *flags]
                    expected = dedupe(content, size, '-p' in flags, '-c' in flags, '-C' in flags)
                    if not test_case(wordlist, args, expected, output, dupfile):
                        return 1
                    cases += 1
            print('[+] %s: %d differential cases succeeded' %
                  (wordlist.name, len(sizes) * len(options)), flush=True)
    print('[+] %d/%d differential cases succeeded' % (cases, cases))
    return 0


if __name__ == '__main__':
    sys.exit(main())
