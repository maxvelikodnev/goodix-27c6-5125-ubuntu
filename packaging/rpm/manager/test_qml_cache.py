#!/usr/bin/python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Load an updated interface in a new process with an older QML disk cache.

All files/cache are temporary and private. No browser, network, helper, host
installation or user cache is accessed. The production loader is exercised.
"""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent


def probe(directory, file_loader):
    from unittest.mock import patch
    sys.path.insert(0, str(directory))
    import main as manager
    from PySide6.QtCore import QObject, QMetaObject, QTimer
    from PySide6.QtQml import QQmlApplicationEngine
    from PySide6.QtQuick import QQuickWindow
    from PySide6.QtQuickControls2 import QQuickStyle
    from PySide6.QtWidgets import QApplication

    # Existing render-harness boundaries: no asynchronous privileged/network work.
    manager.Controller.refresh = lambda self: None
    manager.Controller.check_releases = lambda self: None
    QQuickStyle.setStyle('Fusion')
    app = QApplication([])
    app.setApplicationName('Goodix Manager')
    app.setOrganizationName('Goodix')
    with patch.object(Path, 'read_text', side_effect=FileNotFoundError):
        backend = manager.Controller()
    engine = QQmlApplicationEngine()
    engine.addImportPath('/usr/lib64/qt6/qml')
    engine.rootContext().setContextProperty('backend', backend)
    if file_loader:
        engine.load(str(directory / 'Main.qml'))  # Seed the pre-fix disk cache.
    else:
        manager.load_interface(engine)
    if not engine.rootObjects():
        raise RuntimeError('QML failed to load')
    window = engine.rootObjects()[0]
    result = {}
    with patch.object(manager.QDesktopServices, 'openUrl', return_value=True) as opened:
        def inspect():
            buttons = [obj for obj in window.findChildren(QObject)
                       if obj.property('text') == 'Report a bug']
            result['report_visible'] = bool(buttons and buttons[0].property('visible'))
            if buttons:
                QMetaObject.invokeMethod(buttons[0], 'clicked')
            result['opened_url'] = opened.call_args.args[0].toString() if opened.called else None
            app.quit()
        QTimer.singleShot(250, inspect)
        app.exec()
    print(json.dumps(result, sort_keys=True))


def regression():
    work = HERE.parents[2] / 'development/rpm'
    work.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='qml-cache-test-', dir=work) as temporary:
        root = Path(temporary)
        application, cache = root / 'application', root / 'cache'
        application.mkdir()
        for name in ('main.py', 'model.py', 'io.github.sorguido.GoodixManager.svg'):
            shutil.copyfile(HERE / name, application / name)
        current = (HERE / 'Main.qml').read_bytes()
        older = b''.join(line for line in current.splitlines(keepends=True)
                         if b'text: "Report a bug";' not in line)
        assert current != older, 'Expected the current Report a bug control'
        qml = application / 'Main.qml'
        timestamp = 1700000000  # Two packages with the same reproducible-build mtime.
        environment = dict(os.environ, QT_QPA_PLATFORM='offscreen', XDG_CACHE_HOME=str(cache),
                           QML_DISK_CACHE_PATH=str(cache / 'qmlcache'))
        environment.pop('QML_DISK_CACHE', None)
        environment.pop('QML_DISABLE_DISK_CACHE', None)
        environment.pop('QML_FORCE_DISK_CACHE', None)

        def run_probe(file_loader):
            command = [sys.executable, '-I', '-B', str(Path(__file__).resolve()),
                       '--probe', str(application), 'file' if file_loader else 'production']
            completed = subprocess.run(command, env=environment, capture_output=True,
                                       text=True, check=True, timeout=15)
            assert not completed.stderr, completed.stderr
            return json.loads(completed.stdout)

        qml.write_bytes(older)
        os.utime(qml, (timestamp, timestamp))
        assert not run_probe(True)['report_visible']
        assert any(cache.rglob('*.qmlc')), 'The old process must create a disk cache'
        qml.write_bytes(current)
        os.utime(qml, (timestamp, timestamp))
        updated = run_probe(False)
        assert updated['report_visible'], 'The new process reused the old interface'
        assert updated['opened_url'] == 'https://github.com/sorguido/goodix-27c6-5125/issues'
    print('PASS: new process loads current QML despite old cache and unchanged mtime; Report a bug click opens the expected mocked URL')


if __name__ == '__main__':
    if len(sys.argv) == 4 and sys.argv[1] == '--probe':
        probe(Path(sys.argv[2]), sys.argv[3] == 'file')
    else:
        regression()
