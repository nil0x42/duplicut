#!/usr/bin/env python3

import json
import sys
from pathlib import Path
from urllib.parse import unquote, urlparse


ROOT = Path(__file__).resolve().parents[2]
BUNDLED = {'include/xxhash.h', 'include/thpool.h', 'src/thpool.c'}


def source_path(run, location):
    artifact = location.get('physicalLocation', {}).get('artifactLocation', {})
    if 'uri' not in artifact:
        index = artifact.get('index')
        if not isinstance(index, int) or index < 0:
            return None
        try:
            artifact = run['artifacts'][index]['location']
        except (KeyError, IndexError, TypeError):
            return None
    if artifact.get('uriBaseId') not in (None, '%SRCROOT%'):
        return None
    uri = urlparse(artifact.get('uri', ''))
    if uri.scheme not in ('', 'file') or uri.netloc or not uri.path:
        return None
    path = Path(unquote(uri.path))
    if not path.is_absolute():
        path = ROOT / path
    try:
        return path.resolve().relative_to(ROOT).as_posix()
    except ValueError:
        return None


def filter_run(run):
    components = [run['tool']['driver']] + run['tool'].get('extensions', [])
    rules = {rule['id']: rule for component in components
             for rule in component.get('rules', [])}
    results = []
    for result in run.get('results', []):
        rule_id = result.get('ruleId') or result.get('rule', {}).get('id')
        properties = rules.get(rule_id, {}).get('properties', {})
        locations = result.get('locations', [])
        tags = properties.get('tags') or []
        security = 'security' in tags or 'security-severity' in properties
        if (not tags or security or not locations
                or any(source_path(run, loc) not in BUNDLED for loc in locations)):
            results.append(result)
    run['results'] = results


def main():
    if len(sys.argv) != 2:
        sys.exit('Usage: %s <sarif-file>' % sys.argv[0])
    path = Path(sys.argv[1])
    with path.open(encoding='utf-8') as stream:
        report = json.load(stream)
    for run in report.get('runs', []):
        filter_run(run)
    with path.open('w', encoding='utf-8') as stream:
        json.dump(report, stream, indent=2)
        stream.write('\n')


if __name__ == '__main__':
    main()
