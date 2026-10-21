# SPDX-License-Identifier: GPL-2.0-or-later
"""Pure presentation and release validation. Never accept commands from metadata."""
from dataclasses import dataclass
import json
import re

REPOSITORY = 'https://github.com/sorguido/goodix-27c6-5125'
LATEST_API = 'https://api.github.com/repos/sorguido/goodix-27c6-5125/releases/latest'
SEMVER = r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)'
VERSION = re.compile(SEMVER + r'\Z')
STATES = {'absent', 'installed', 'legacy', 'blocked'}
MESSAGES = {
    'ready': 'The local payload is ready.',
    'not_installed': 'The driver has not been activated.',
    'legacy_unqualified': 'Earlier driver detected. Migration is not yet qualified; Update is disabled. Ordinary removal is available.',
    'source_mismatch': 'Installed and local builds differ at the same version. Update is blocked; ordinary verified removal is available.',
    'version_unknown': 'The installed version cannot be established. Update is disabled; ordinary verified removal is available.',
    'up_to_date': 'The installed driver matches the local payload.',
    'update_available': 'A newer verified local payload is ready to apply.',
    'downgrade_blocked': 'The active driver is newer than this payload. Downgrades are disabled.',
    'authorization_required': 'Administrative authorization is required to verify this state.',
    'invalid_request': 'The helper rejected the requested operation.',
    'busy': 'Another lifecycle operation is running. Wait, then verify status.',
    'operation_failed': 'The operation failed. Verify status before continuing.',
    'rollback_complete': 'The operation failed; the previous software was restored.',
    'removed': 'The driver was removed. Goodix Manager and protected data are preserved.',
    'runtime_changed': 'The runtime changed during preparation. Verify status again.',
    'missing_dependencies': 'Required Fedora runtime dependencies are missing.',
    'absent': 'The driver has not been activated.',
    'installed': 'The driver installation passed its integrity checks.',
    'legacy': 'An earlier driver is installed. Migration requires new enrollment.',
    'blocked': 'Installation state could not be verified. Use the documented recovery procedure.',
    'unsupported_host': 'This Fedora release or architecture is not supported by this package.',
    'invalid_payload': 'The local payload could not be verified. Reinstall a trusted RPM.',
    'integrity_failed': 'Installed files failed verification. Review the recovery documentation.',
    'success': 'Operation completed. Review the driver status below.',
    'failed': 'Operation failed. Review the verified status before trying again.',
    'rolled_back': 'Operation failed; the previous software was restored.',
    'rollback_failed': 'Recovery is incomplete. Stop and use the documented TTY recovery procedure.',
    'authorization_failed': 'Authorization was cancelled or denied. No success was reported.',
    'helper_unavailable': 'The installed manager helper is unavailable.',
    'invalid_response': 'The helper did not return a valid status. Actions are disabled.',
}


def version_tuple(value):
    if not isinstance(value, str) or len(value) > 48 or not VERSION.fullmatch(value):
        raise ValueError('invalid stable version')
    return tuple(int(part) for part in value.split('.'))


@dataclass(frozen=True)
class Status:
    state: str = 'blocked'
    runtime_version: str | None = None
    payload_version: str = ''
    package_version: str = ''
    sensor_present: bool | None = None
    message_code: str = 'invalid_response'
    reboot_required: bool = False
    enrollment_required: bool = False

    @property
    def action(self):
        if self.state == 'absent':
            return 'install'
        if self.state == 'installed' and self.runtime_version is not None:
            if version_tuple(self.runtime_version) < version_tuple(self.payload_version):
                return 'update'
        return ''

    @property
    def title(self):
        if self.state == 'absent':
            return 'Ready to install'
        if self.state == 'legacy':
            return 'Earlier driver detected'
        if self.state == 'blocked':
            return 'Attention required'
        if self.action == 'update':
            return 'Ready to apply update'
        if version_tuple(self.runtime_version) > version_tuple(self.payload_version):
            return 'Newer runtime installed'
        return 'Up to date'

    @classmethod
    def parse(cls, data):
        if not isinstance(data, dict) or data.get('schema') != 1 or data.get('state') not in STATES:
            raise ValueError('invalid status')
        payload = data.get('payload_version')
        unknown_package = data['state'] == 'blocked' and data.get('result') == 'blocked' and payload == ''
        if not unknown_package:
            version_tuple(payload)
        runtime = data.get('runtime_version')
        if runtime is not None:
            version_tuple(runtime)
        if (data['state'] == 'absent' and runtime is not None or
                data['state'] == 'installed' and runtime is None):
            raise ValueError('inconsistent runtime state')
        sensor = data.get('sensor_present')
        if sensor is not None and type(sensor) is not bool:
            raise ValueError('invalid sensor status')
        for field in ('reboot_required', 'enrollment_required'):
            if type(data.get(field, False)) is not bool:
                raise ValueError('invalid flag')
        package = data.get('package_version', payload)
        if not (unknown_package and package == '') and (not isinstance(package, str) or not re.fullmatch(SEMVER + r'(?:-[1-9][0-9]*\.fc44)?', package)):
            raise ValueError('invalid package version')
        code = data.get('message_code', 'ready')
        if code not in MESSAGES:
            code = 'blocked' if data['state'] == 'blocked' else 'ready'
        return cls(data['state'], runtime, payload, package, sensor, code,
                   data.get('reboot_required', False), data.get('enrollment_required', False))

    def diagnostic(self):
        # Explicit fields only: never export helper stdout/stderr or remote text.
        return json.dumps({
            'schema': 1, 'state': self.state, 'runtime_version': self.runtime_version,
            'payload_version': self.payload_version, 'package_version': self.package_version,
            'sensor_present': self.sensor_present, 'message_code': self.message_code,
            'reboot_required': self.reboot_required, 'enrollment_required': self.enrollment_required,
        }, indent=2)


@dataclass(frozen=True)
class Release:
    version: str
    packaging_release: int
    page_url: str
    asset_url: str

    @classmethod
    def parse(cls, data):
        if not isinstance(data, dict) or data.get('draft') is not False or data.get('prerelease') is not False:
            raise ValueError('not a stable public release')
        tag = data.get('tag_name')
        if not isinstance(tag, str):
            raise ValueError('missing tag')
        version = tag[1:] if tag.startswith('v') else tag
        version_tuple(version)
        page = REPOSITORY + '/releases/tag/' + tag
        if data.get('html_url') != page:
            raise ValueError('untrusted release URL')
        assets = data.get('assets')
        if not isinstance(assets, list) or len(assets) > 128:
            raise ValueError('invalid assets')
        found = []
        pattern = re.compile(r'goodix-manager-' + re.escape(version) + r'-([1-9][0-9]{0,5})\.fc44\.x86_64\.rpm\Z')
        for asset in assets:
            if not isinstance(asset, dict) or not isinstance(asset.get('name'), str):
                raise ValueError('malformed asset')
            match = pattern.fullmatch(asset['name'])
            if not match:
                continue
            url = REPOSITORY + '/releases/download/' + tag + '/' + asset['name']
            if (asset.get('browser_download_url') != url or asset.get('state') != 'uploaded'
                    or type(asset.get('size')) is not int or asset['size'] <= 0):
                raise ValueError('invalid download')
            found.append((int(match.group(1)), url))
        if not found or len({item[0] for item in found}) != len(found):
            raise ValueError('missing or ambiguous compatible RPM')
        revision, url = max(found)
        return cls(version, revision, page, url)

    def newer_than(self, status):
        candidate = version_tuple(self.version)
        if status.runtime_version and candidate < version_tuple(status.runtime_version):
            return False
        local = version_tuple(status.payload_version)
        if candidate != local:
            return candidate > local
        match = re.fullmatch(SEMVER + r'-([1-9][0-9]*)\.fc44', status.package_version)
        return bool(match and self.packaging_release > int(match.group(4)))
