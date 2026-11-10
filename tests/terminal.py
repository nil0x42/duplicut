import errno
import fcntl
import os
import struct
import subprocess
import sys
import termios
import time
from concurrent.futures import ThreadPoolExecutor


def terminal_reader(fd):
    chunks = []
    while True:
        try:
            block = os.read(fd, 65536)
        except OSError as error:
            if error.errno == errno.EIO:
                break
            raise
        if not block:
            break
        chunks.append(block)
    return b''.join(chunks)


def terminal_session(command):
    os.setsid()
    fcntl.ioctl(2, termios.TIOCSCTTY, 0)
    before = termios.tcgetattr(2)
    result = subprocess.run(command, timeout=10)
    # Keep the controlling session alive until its settings have been checked.
    after = termios.tcgetattr(2)
    # macOS sets PENDIN internally when canonical input is restored.
    before[3] &= ~termios.PENDIN
    after[3] &= ~termios.PENDIN
    assert after == before, ('terminal settings were not restored', before, after)
    sys.exit(result.returncode if result.returncode >= 0 else 128 - result.returncode)


def terminal(command, content, columns=120, delay=0):
    master, slave = os.openpty()
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', 24, columns, 0, 0))
    try:
        process = subprocess.Popen([sys.executable, __file__, *command],
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=slave)
        with ThreadPoolExecutor(max_workers=1) as pool:
            reader = pool.submit(terminal_reader, master)
            try:
                if delay:
                    time.sleep(delay)
                output, unused = process.communicate(content, timeout=12)
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait()
                os.close(slave)
            log = reader.result(timeout=5)
        return process.returncode, output, log
    finally:
        os.close(master)


if __name__ == '__main__':
    terminal_session(sys.argv[1:])
