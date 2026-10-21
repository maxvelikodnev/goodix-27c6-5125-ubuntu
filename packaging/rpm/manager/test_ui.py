#!/usr/bin/python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Qt/Kirigami render smoke test with all privilege/network entry points replaced."""
import argparse
import os
from pathlib import Path
import sys

os.environ['QT_QPA_PLATFORM'] = 'offscreen'
sys.path.insert(0, str(Path(__file__).resolve().parent))
from main import Controller
from model import Status
from PySide6.QtCore import QTimer, qInstallMessageHandler
from PySide6.QtQml import QQmlApplicationEngine
from PySide6.QtQuick import QQuickWindow
from PySide6.QtQuickControls2 import QQuickStyle
from PySide6.QtWidgets import QApplication


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--render-dir', type=Path)
    args = parser.parse_args()
    Controller.refresh = lambda self: None
    Controller.check_releases = lambda self: None
    qInstallMessageHandler(lambda kind, context, message: print('Qt:', message, file=sys.stderr))
    QQuickStyle.setStyle('Fusion')
    app = QApplication([])
    backend = Controller()
    backend.status = Status.parse({'schema': 1, 'state': 'installed', 'runtime_version': '3.0.0',
                                   'payload_version': '3.1.0', 'package_version': '3.1.0-1.fc44',
                                   'sensor_present': False, 'message_code': 'update_available'})
    backend.notice = 'A newer verified local payload is ready to apply.'
    backend.remote_state = 'unavailable'
    backend.remote_message = 'Check unavailable · local actions remain available'
    engine = QQmlApplicationEngine()
    engine.addImportPath('/usr/lib64/qt6/qml')
    engine.rootContext().setContextProperty('backend', backend)
    engine.load(str(Path(__file__).with_name('Main.qml')))
    if not engine.rootObjects():
        return 1
    window = engine.rootObjects()[0]
    results = []
    def save(name):
        picture = window.grabWindow()
        assert not picture.isNull()
        if args.render_dir:
            args.render_dir.mkdir(parents=True, exist_ok=True)
            assert picture.save(str(args.render_dir / ('manager-' + name + '.png')))
        results.append(picture)
    def light():
        window.setProperty('themeChoice', 1)
        QTimer.singleShot(250, save_light)
    def save_light():
        save('light')
        window.setProperty('themeChoice', 2)
        QTimer.singleShot(250, save_dark)
    def save_dark():
        save('dark')
        window.setWidth(520)
        window.setHeight(640)
        QTimer.singleShot(250, save_compact)
    def save_compact():
        save('compact')
        app.quit()
    QTimer.singleShot(250, light)
    QTimer.singleShot(5000, app.quit)
    app.exec()
    assert len(results) == 3 and results[0] != results[1], 'Themes failed to produce distinct frames'
    assert results[2].width() == 520
    print('PASS: light, dark and compact QML render; no privileged or network calls')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
