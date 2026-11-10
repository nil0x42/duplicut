import os
import subprocess


def rejected(program, source, output, duplicates):
    result = subprocess.run([program, str(source), '-o', str(output), '-D', str(duplicates)],
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5)
    assert result.returncode != 0, result.stderr


def test_existing_aliases(program, tmp_path):
    for pair in ('infile-outfile', 'infile-dupfile', 'outfile-dupfile'):
        for link in ('same', 'hardlink', 'symlink'):
            directory = tmp_path / (pair + '-' + link)
            directory.mkdir()
            source, output, duplicates = (directory / name for name in ('input', 'output', 'duplicates'))
            source.write_bytes(b'alpha\nbeta\nalpha\n')
            if pair == 'infile-outfile':
                target, alias = source, output
                duplicates.write_bytes(b'keep duplicates\n')
            elif pair == 'infile-dupfile':
                target, alias = source, duplicates
                output.write_bytes(b'keep output\n')
            else:
                output.write_bytes(b'keep output\n')
                target, alias = output, duplicates
            if link == 'same':
                if pair == 'infile-outfile':
                    output = target
                else:
                    duplicates = target
            elif link == 'hardlink':
                os.link(target, alias)
            else:
                alias.symlink_to(target)
            saved = [(path, path.read_bytes()) for path in (source, output, duplicates)]
            rejected(program, source, output, duplicates)
            for path, content in saved:
                assert path.read_bytes() == content, (pair, link, path)


def test_new_output_aliases(program, tmp_path):
    source = tmp_path / 'input'
    content = b'alpha\nbeta\nalpha\n'
    source.write_bytes(content)
    for link in ('same', 'relative', 'symlink'):
        output = tmp_path / ('new-' + link)
        if link == 'same':
            duplicates = output
        elif link == 'relative':
            duplicates = str(tmp_path) + '/./new-relative'
        else:
            duplicates = tmp_path / 'link'
            duplicates.symlink_to(output)
        rejected(program, source, output, duplicates)
        assert source.read_bytes() == content


def test_failed_open_preserves_outputs(program, tmp_path):
    source, output = tmp_path / 'input', tmp_path / 'output'
    source.write_bytes(b'alpha\nbeta\nalpha\n')
    output.write_bytes(b'keep output\n')
    rejected(program, source, output, tmp_path)
    assert source.read_bytes() == b'alpha\nbeta\nalpha\n'
    assert output.read_bytes() == b'keep output\n'


def test_output_truncation(program, tmp_path):
    source, output, duplicates = (tmp_path / name for name in ('input', 'output', 'duplicates'))
    source.write_bytes(b'alpha\nbeta\nalpha\n')
    output.write_bytes(b'keep output\n')
    duplicates.write_bytes(b'keep duplicates\n')
    subprocess.run([program, str(source), '-o', str(output), '-D', str(duplicates)], check=True, timeout=5)
    assert output.read_bytes() == b'alpha\nbeta\n'
    assert duplicates.read_bytes() == b'alpha\n'
    source.write_bytes(b'')
    subprocess.run([program, str(source), '-o', str(output), '-D', str(duplicates)], check=True, timeout=5)
    assert not output.read_bytes() and not duplicates.read_bytes()
