#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Compile the pinned Milan recipe from a self-contained source snapshot."""
import hashlib
import json
from pathlib import Path
import shlex

C1_PIN = '4f48c482f35976ea4fac27d5e598730581c4f380'
MILAN_PIN = 'e68eb725b1a7845b401b1a3c9b0790921f12133e'
RECIPE_SHA256 = 'c992bf284cc4d615807a635bf53e8f8ac96746bc3fbcf22e23393bae3ad5250b'
NATIVE_SOURCE = Path('production/milan-native')
NATIVE_INCLUDE = Path('package/experiments/milan521d/native')
RECIPE_PATH = Path('package/native-engine/recipe.json')
PUBLICATION_GATE = ('Milan native sources unavailable: the recorded MIT/RVA publication '
                    'gate is unresolved; no SIGFM fallback or private-source lookup is allowed')


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def relative_path(name):
    return isinstance(name, str) and name != '' and not Path(name).is_absolute() and \
        all(part not in ('', '.', '..') for part in name.split('/'))


def validate_sources(root):
    """No commands or host changes; safe for installer preflight and source archives."""
    source = Path(root) / NATIVE_SOURCE
    require(source.is_dir() and not source.is_symlink() and
            source.resolve().is_relative_to(Path(root).resolve()), PUBLICATION_GATE)
    manifest_path = source / 'source-manifest.json'
    require(manifest_path.is_file() and not manifest_path.is_symlink(),
            'Milan native source manifest is missing')
    manifest = json.loads(manifest_path.read_bytes())
    require(isinstance(manifest, dict) and set(manifest) ==
            {'schema', 'c1_commit', 'milan_commit', 'files'} and
            type(manifest['schema']) is int and manifest['schema'] == 1 and
            manifest['c1_commit'] == C1_PIN and manifest['milan_commit'] == MILAN_PIN and
            isinstance(manifest['files'], dict), 'invalid Milan native source manifest')
    files = manifest['files']
    require({RECIPE_PATH.as_posix(), 'NOTICES.txt'} <= set(files),
            'Milan recipe or original license notices missing from source manifest')
    actual = set()
    for path in source.rglob('*'):
        require(not path.is_symlink(), 'Milan native sources cannot contain symlinks')
        require(path.is_dir() or path.is_file(), 'invalid Milan native source entry')
        if path.is_file() and path != manifest_path:
            actual.add(path.relative_to(source).as_posix())
    require(actual == set(files), 'Milan native source inventory drift')
    for name, checksum in files.items():
        require(relative_path(name) and isinstance(checksum, str) and len(checksum) == 64 and
                all(c in '0123456789abcdef' for c in checksum), 'invalid Milan source path/hash')
        path = source / name
        require(path.is_file() and path.resolve().is_relative_to(source.resolve()) and
                digest(path) == checksum, 'Milan native source drift: ' + name)
    require(digest(source / RECIPE_PATH) == RECIPE_SHA256, 'Milan native recipe is not pinned')
    recipe = json.loads((source / RECIPE_PATH).read_bytes())
    for name, checksum in recipe['source_sha256'].items():
        require(files.get(name) == checksum, 'Milan recipe input is not pinned: ' + name)
    return manifest


def generate_enums(source, include, run):
    # Reuse the C1/M4 templates, generating outputs in the temporary build only.
    headers = source / 'libfprint/libfprint'
    public = ['fp-context.h', 'fp-device.h', 'fp-image-device.h', 'fp-image.h', 'fp-print.h']
    private = ['fpi-assembling.h', 'fpi-byte-reader.h', 'fpi-byte-utils.h',
               'fpi-byte-writer.h', 'fpi-compat.h', 'fpi-context.h', 'fpi-device.h',
               'fpi-image-device.h', 'fpi-image.h', 'fpi-log.h', 'fpi-minutiae.h',
               'fpi-print.h', 'fpi-usb-transfer.h', 'fpi-ssm.h', 'fpi-spi-transfer.h']
    for name, names in (('fp-enums.h', public), ('fpi-enums.h', private)):
        text = run('glib-mkenums', '--fhead', '#pragma once\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n',
                   '--fprod', '\n/* enumerations from "@basename@" */\n', '--vhead',
                   '\nGType @enum_name@_get_type (void);\n#define @ENUMPREFIX@_TYPE_@ENUMSHORT@ (@enum_name@_get_type())',
                   '--ftail', '\nG_END_DECLS', *(headers / item for item in names))
        (include / name).write_text(text + '\n')


def build_native(root, work, run):
    manifest = validate_sources(root)
    source = Path(root) / NATIVE_SOURCE
    recipe = json.loads((source / RECIPE_PATH).read_bytes())
    native = work / 'native'
    native.mkdir()
    include, objects = native / 'include', native / 'objects'
    include.mkdir()
    objects.mkdir()
    generate_enums(source, include, run)
    commands, compiled, dependencies = [], [], {}
    for index, original in enumerate(recipe['commands']):
        if '-c' not in original:
            require('-shared' in original, 'unexpected Milan native recipe command')
            continue
        command = [part.replace('@ROOT@', str(source)) for part in original]
        obj = objects / f'{index:03d}.o'
        depfile = obj.with_suffix('.d')
        command[command.index('-o') + 1] = str(obj)
        command += ['-I', str(include), '-MMD', '-MF', str(depfile),
                    f'-ffile-prefix-map={source}=/usr/src/goodix-milan',
                    f'-ffile-prefix-map={work}=/usr/src/goodix-build']
        run(*command)
        commands.append(command)
        compiled.append(obj)
        for name in shlex.split(depfile.read_text().replace('\\\n', ''))[1:]:
            path = Path(name).resolve()
            require(path.is_relative_to(source.resolve()) or path.is_relative_to(work.resolve()) or
                    path.is_relative_to('/usr/include') or path.is_relative_to('/usr/lib64'),
                    'native build accessed an unexpected dependency: ' + str(path))
            name = str(path)
            if path.is_relative_to(source.resolve()):
                name = str(NATIVE_SOURCE / path.relative_to(source.resolve()))
            elif path.is_relative_to(work.resolve()):
                name = str(Path('build') / path.relative_to(work.resolve()))
            dependencies[name] = digest(path)
    archive = native / 'libgoodix521d-native.a'
    run('ar', 'rcsD', archive, *compiled)
    commands.append(['ar', 'rcsD', str(archive), *(str(obj) for obj in compiled)])
    return archive, source / NATIVE_INCLUDE, {
        'c1_commit': C1_PIN, 'milan_commit': MILAN_PIN,
        'native_recipe_sha256': RECIPE_SHA256, 'native_pic_objects': len(compiled),
        'native_source_manifest_sha256': digest(source / 'source-manifest.json'),
        'native_archive_sha256': digest(archive), 'native_dependencies': dependencies,
        'native_commands': [[part.replace(str(source), '@NATIVE_SOURCE@')
                            .replace(str(work), '@BUILD@') for part in command]
                            for command in commands],
    }
