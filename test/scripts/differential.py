#!/usr/bin/env python3

import os
import runpy
import shlex
import subprocess
import sys
import tempfile
from collections import Counter
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from threading import Event


dedupe = runpy.run_path(str(Path(__file__).with_name('reference-dedupe.py')))['dedupe']


def default_jobs():
    try:
        cpus = len(os.sched_getaffinity(0)) or 1
    except (AttributeError, OSError):
        cpus = os.cpu_count() or 1
    # Each duplicut process also has its own threads.
    return (cpus + 1) // 2


def test_case(wordlist, content, size, flags, stop):
    if stop.is_set():
        return None
    args = ['-l', str(size), *flags]
    expected = dedupe(content, size, '-p' in flags, '-c' in flags, '-C' in flags)
    with tempfile.TemporaryDirectory(prefix='duplicut-tests-') as directory:
        output = Path(directory) / 'output'
        dupfile = Path(directory) / 'duplicates'
        command = ['./duplicut', '-o', str(output), '-D', str(dupfile), '-m', '64K', *args]
        stderr = b''
        if stop.is_set():
            return None
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
        return {
            'wordlist': wordlist,
            'args': args,
            'reason': reason,
            'stderr': stderr,
            'expected': expected,
            'actual': tuple(path.read_bytes() if path.is_file() else None
                            for path in (output, dupfile)),
        }


def report_failure(failure):
    command = ['./duplicut', '-o', 'differential-unique-duplicut.out',
               '-D', 'differential-duplicates-duplicut.out', '-m', '64K', *failure['args']]
    print('[!] %s: %s < %s' % (failure['reason'], shlex.join(command),
                              shlex.quote(str(failure['wordlist']))), file=sys.stderr)
    if failure['stderr']:
        print(failure['stderr'].decode('utf-8', errors='backslashreplace'),
              end='', file=sys.stderr)
    for kind, expected, content in zip(('unique', 'duplicates'),
                                       failure['expected'], failure['actual']):
        reference = Path('differential-%s-reference.out' % kind)
        actual = Path('differential-%s-duplicut.out' % kind)
        reference.write_bytes(expected)
        actual.unlink(missing_ok=True)
        if content is not None:
            actual.write_bytes(content)
        print('[!] Outputs: %s, %s' % (reference, actual), file=sys.stderr)


def main():
    wordlists = sorted(path for path in Path('test/wordlists').glob('*.txt') if path.is_file())
    if not wordlists:
        print('[!] No test wordlists found', file=sys.stderr)
        return 1

    sizes = (1, 5, 14, 15, 40, 65, 128, 2000, 4095)
    options = ((), ('-p',), ('-c',), ('-p', '-c'), ('-C',), ('-p', '-C'))
    per_wordlist = len(sizes) * len(options)
    total = len(wordlists) * per_wordlist
    try:
        jobs = int(os.environ.get('TEST_JOBS', default_jobs()))
        if jobs < 1:
            raise ValueError
    except ValueError:
        print('[!] TEST_JOBS must be a positive integer', file=sys.stderr)
        return 1

    jobs = min(jobs, total)
    print('[*] Running %d differential cases with %d jobs' % (total, jobs), flush=True)
    contents = [(wordlist, wordlist.read_bytes()) for wordlist in wordlists]
    stop = Event()
    done = Counter()
    cases = 0
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        futures = {pool.submit(test_case, wordlist, content, size, flags, stop): wordlist
                   for wordlist, content in contents for size in sizes for flags in options}
        try:
            for future in as_completed(futures):
                result = future.result()
                if result is None:
                    continue
                if result is not True:
                    stop.set()
                    report_failure(result)
                    return 1
                wordlist = futures[future]
                done[wordlist] += 1
                cases += 1
                if done[wordlist] == per_wordlist:
                    print('[+] %s: %d differential cases succeeded' %
                          (wordlist.name, per_wordlist), flush=True)
        finally:
            stop.set()
            for future in futures:
                future.cancel()
    print('[+] %d/%d differential cases succeeded' % (cases, total))
    return 0


if __name__ == '__main__':
    sys.exit(main())
