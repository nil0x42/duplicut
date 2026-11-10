#!/usr/bin/env python3

import math
import os
import re
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path

from build import build, build_debug, build_faults


def run(program, directory, content, memory='8M', env=None):
    source = directory / 'input'
    output = directory / 'output'
    source.write_bytes(content)
    result = subprocess.run([str(program), str(source), '-o', str(output),
                             '-m', memory, '-l', '4095', '-t', '1'],
                            stderr=subprocess.PIPE, timeout=30, env=env)
    assert result.returncode == 0, result.stderr
    log = result.stderr.decode()
    if not content:
        assert not output.read_bytes()
        return {}, log, b''
    fields = {}
    for name in ('hmap_size', 'chunk_size'):
        fields[name] = int(re.search(r'g_conf\.' + name + r':.*?\((\d+)', log)[1])
    fields['budget'] = int(re.search(r'g_conf\.dedupe_memory:\s+(\d+)', log)[1])
    fields['sample'] = float(re.search(r'line estimate: ([\d.]+)', log)[1])
    sampled = int(re.search(r'([\d]+) sampled bytes', log)[1])
    assert sampled <= min(len(content), 65536)
    table = fields['hmap_size'] * 8
    assert table <= fields['budget']
    assert table + fields['chunk_size'] <= fields['budget'] * 1.75 + 1
    assert all(fields['hmap_size'] % i for i in range(2, math.isqrt(fields['hmap_size']) + 1))
    assert '50% target, 70% maximum' in log
    assert 'DLOG1' in log and 'config.c:' in log
    assert 'legacy' not in log
    return fields, log, output.read_bytes()


def main():
    root = Path.cwd().resolve()
    with tempfile.TemporaryDirectory(prefix='duplicut-configuration-', dir=root) as folder:
        directory = Path(folder)
        program = (Path(sys.argv[1]).resolve() if len(sys.argv) > 1
                   else build_debug(root, directory))
        words = b''.join(('%03x\n' % i).encode() for i in range(4096))
        fields, log, output = run(program, directory, words * 2, '64K')
        assert fields['sample'] == 4 and output == words
        assert fields['chunk_size'] <= fields['hmap_size'] * 3 + 1
        boundary_words = words[:10800]
        content = boundary_words + boundary_words[:1200]
        fields, log, output = run(program, directory, content, '64K')
        assert fields['chunk_size'] == len(content) and output == boundary_words
        assert 'mmap-chunks load limit:' not in log
        print('[+] nearest chunk count without an unnecessary extra pass')
        long_words = b''.join(('%08x' % i + 'x' * 120 + '\n').encode() for i in range(6000))
        fields, log, output = run(program, directory, long_words * 2, '64M')
        assert fields['sample'] == 129 and output == long_words
        assert fields['chunk_size'] == len(long_words) * 2
        assert fields['budget'] > fields['hmap_size'] * 8 * 10
        assert 'dedupe engine: mmap-chunks' in log
        print('[+] file-bound table, whole-file chunk and independent engine budget')
        content = (b'z' * 4095 + b'\n') * 512
        fields, log, output = run(program, directory, content, '2M')
        assert fields['chunk_size'] < len(content) and output == content[:4096]
        print('[+] working-set bound with a small file-bound table')

        content = words + (b'z' * 127 + b'\n') * (240 * 1024 // 128)
        fields, log, output = run(program, directory, content, '1M')
        assert fields['sample'] == 4
        assert fields['chunk_size'] == len(content)
        assert output == words + b'z' * 127 + b'\n'
        assert fields['hmap_size'] * 8 > fields['budget'] * .99
        print('[+] minimum window density instead of global mean')

        alphabet = bytes(range(33, 127))
        words = [bytes((alphabet[i // 94 ** 2], alphabet[i // 94 % 94], alphabet[i % 94]))
                 for i in range(24576)]
        short = b''.join(word + b'\n' for word in words) * 2
        long_block = (b'z' * 4095 + b'\n') * 4
        content = b''.join(long_block + short[i * 65536:(i + 1) * 65536] for i in range(3))
        content += long_block
        fields, log, output = run(program, directory, content)
        assert fields['sample'] == 4096 and fields['chunk_size'] == len(content)
        assert log.count('mmap-chunks load limit:') >= 3
        assert output == b'z' * 4095 + b'\n' + b''.join(word + b'\n' for word in words)
        print('[+] underestimated density and repeated bounded saturation')

        for content in (b'', b'\0ignored', b'\n\r\n', b'x' * 5000):
            fields, log, output = run(program, directory, content, '64K')
            assert (not fields or fields['sample'] == 8) and not output
        fields, log, output = run(program, directory, b'A\r\nB\nC')
        assert abs(fields['sample'] - 2) < .001 and output == b'A\nB\nC\n'
        print('[+] empty samples, filters, CRLF and a final line without LF')

        if len(sys.argv) > 1:
            return
        partitions = build(root, directory, 'partitions', '-DDEBUG=1', '-DPARTITION_MIN_CHUNKS=0')
        fields, log, output = run(partitions, directory, long_words * 2)
        assert output == long_words
        assert 'engine candidate: disk-partitions' in log
        assert 'dedupe engine: disk-partitions' in log
        for phase in ('counting partitions', 'writing partitions', 'deduping partitions', 'marking duplicates'):
            assert 'disk-partitions phase: ' + phase in log
        assert 'update_status(CHUNK_START)' not in log
        assert 'partition memory:' in log
        print('[+] initial partition plan, final allocations and phases')

        if sys.platform == 'linux':
            library = build_faults(root, directory)
            reads = directory / 'memory-reads'
            env = {**os.environ, 'LD_PRELOAD': str(library), 'DUPLICUT_TEST_FAULT': 'space',
                   'DUPLICUT_TEST_AVAILABLE': str(128 * 1024 * 1024),
                   'DUPLICUT_TEST_MEM_LOG': str(reads)}
            fields, log, output = run(partitions, directory, long_words * 2, env=env)
            assert output == long_words
            assert reads.read_text().splitlines() == ['MemAvailable']
            assert 'engine candidate: disk-partitions' in log
            assert 'disk-partitions unavailable:' in log
            assert 'dedupe engine: mmap-chunks' in log
            assert 'dedupe engine: disk-partitions' not in log
            assert 'update_status(CHUNK_START)' in log
            print('[+] one memory snapshot and explicit disk preflight fallback')

        status = directory / 'status'
        compiler = shlex.split(os.environ.get('CC', 'cc'))
        subprocess.run([*compiler, '-std=gnu99', '-O2', '-DNDEBUG', '-Wall', '-Wextra',
                        '-I' + str(root / 'include'), str(root / 'test/scripts/status.c'),
                        str(root / 'src/status.c'), str(root / 'src/cprintferr.c'),
                        '-pthread', '-lm', '-o', str(status)], check=True)
        for flags in ((), ('fallback',)):
            result = subprocess.run([str(status), *flags], stderr=subprocess.PIPE, timeout=5)
            assert result.returncode == 0
            text = result.stderr.decode()
            assert not re.search(r'\b(?:inf|nan)\b', text, re.IGNORECASE)
            assert '94.0%' in text and '99.0%' in text and '100.0%' in text
            assert '(done: took 0.123s)' in text and '(done: took 0.456s)' in text
            assert '(done: took 1h 0m)' in text and 'ETA:' not in text
            if not flags:
                assert '(done: took 1s)' in text and '(done: took 1m 0s)' in text
            assert 'disk-partitions' in text
            if flags:
                assert 'mmap-chunks' in text and 'task 1/13' in text
        print('[+] progress, cleanup transition and fallback task initialization')


if __name__ == '__main__':
    main()
