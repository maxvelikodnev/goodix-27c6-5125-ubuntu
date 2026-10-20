#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Build the pinned, self-contained Chicago computational sources; no device I/O."""
import hashlib
import json
from pathlib import Path
import shlex

CHICAGO_PIN = '010a665f54089a1632b1ab7be588b316ace934e2'
NATIVE_SOURCE = Path('production/chicago-native')
INCOMPLETE_SOURCE_SET = 'Chicago native source set is incomplete; restore production/chicago-native from the source checkout'

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def require(value, message):
    if not value:
        raise RuntimeError(message)

def validate_sources(root):
    root = Path(root).resolve()
    source = root / NATIVE_SOURCE
    require(source.is_dir() and not source.is_symlink(), INCOMPLETE_SOURCE_SET)
    manifest_path = source / 'source-manifest.json'
    require(manifest_path.is_file() and not manifest_path.is_symlink(), INCOMPLETE_SOURCE_SET)
    manifest = json.loads(manifest_path.read_bytes())
    require(set(manifest) == {'schema', 'upstream_commit', 'files'} and manifest['schema'] == 1 and
            manifest['upstream_commit'] == CHICAGO_PIN and isinstance(manifest['files'], dict), 'invalid Chicago manifest')
    files = manifest['files']
    actual = set()
    for path in source.rglob('*'):
        require(not path.is_symlink() and (path.is_dir() or path.is_file()), 'invalid Chicago source entry')
        if path.is_file() and path != manifest_path:
            actual.add(path.relative_to(source).as_posix())
    require(actual == set(files), 'Chicago native source inventory drift')
    require({'LICENSE', 'NOTICES.txt', 'PROVENANCE.md'} <= actual, 'Chicago notices missing')
    for name, sha in files.items():
        require(isinstance(name, str) and not Path(name).is_absolute() and
                all(p not in ('', '.', '..') for p in name.split('/')), 'invalid Chicago source path')
        require(isinstance(sha, str) and len(sha) == 64 and digest(source / name) == sha, 'Chicago native source drift: ' + name)
    return manifest

def build_native(root, work, run):
    validate_sources(root)
    source = Path(root) / NATIVE_SOURCE
    native = Path(work) / 'native'
    native.mkdir()
    flags = shlex.split(run('pkg-config', '--cflags', 'gio-2.0'))
    objects, commands, dependencies = [], [], {}
    for i, path in enumerate(sorted((source / 'chicago').glob('*.c')) + sorted((source / 'common').glob('*.c'))):
        obj = native / ('%02d.o' % i)
        dep = obj.with_suffix('.d')
        command = ['cc', '-std=c11', '-O2', '-g', '-fPIC', '-Wall', '-Wextra',
                   '-I', str(source / 'chicago'), '-I', str(source / 'common'), *flags,
                   '-MMD', '-MF', str(dep), '-ffile-prefix-map=' + str(source) + '=/usr/src/goodix-chicago',
                   '-c', str(path), '-o', str(obj)]
        run(*command)
        commands.append(command)
        objects.append(obj)
        for entry in shlex.split(dep.read_text().replace('\\\n', ''))[1:]:
            p = Path(entry).resolve()
            require(p.is_relative_to(source.resolve()) or p.is_relative_to('/usr/include') or
                    p.is_relative_to('/usr/lib64') or p.is_relative_to('/usr/lib'), 'unexpected native dependency')
            dependencies[str(NATIVE_SOURCE / p.relative_to(source.resolve())) if p.is_relative_to(source.resolve()) else str(p)] = digest(p)
    archive = native / 'libgoodix-chicago-native.a'
    run('ar', 'rcsD', archive, *objects)
    return archive, source / 'chicago', {
        'chicago_commit': CHICAGO_PIN, 'native_pic_objects': len(objects),
        'native_source_manifest_sha256': digest(source / 'source-manifest.json'),
        'native_archive_sha256': digest(archive), 'native_dependencies': dependencies,
        'native_commands': [[str(p).replace(str(source), '@NATIVE_SOURCE@').replace(str(work), '@BUILD@') for p in c] for c in commands],
    }
