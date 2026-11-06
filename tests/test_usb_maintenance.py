"""Linux fixtures exercise file-level install/rollback; hardware/mount guards are separate."""
import hashlib
import importlib.util
import io
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('release_usb', ROOT / 'tools/build_release.py')
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


@unittest.skipUnless(sys.platform.startswith('linux'), 'Linux shell fixture')
class UsbMaintenance(unittest.TestCase):
    def test_init_guard_contexts_and_failed_write_restoration(self):
        with tempfile.TemporaryDirectory() as tmp:
            binary = str(Path(tmp) / 'guard-test')
            subprocess.run(['gcc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                            '-DINIT_GUARD_TEST', str(ROOT / 'installer/init-guard.c'),
                            '-o', binary], check=True, capture_output=True)
            result = subprocess.run([binary], check=True, capture_output=True)
            self.assertIn(b'I/O failures passed', result.stdout)

    def test_shutdown_targets_only_positive_userspace_pids(self):
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            for pid, value in {'1': '100 50', '2': '0 0', '474': '0 0',
                               '600': '108 5', '601': '0 0', '602': 'bad 0',
                               '603': '12 2', '604': ''}.items():
                directory = base / pid; directory.mkdir()
                (directory / 'statm').write_text(value + '\n')
            source = (ROOT / 'installer/usb-pid1.sh').read_text()
            function = source.split('stop_userspace() {', 1)[1].split('\n}', 1)[0]
            # Fixture replaces only /proc; production has no alternative proc path.
            function = function.replace('/proc/[0-9]*', str(base) + '/[0-9]*')
            script = ('kill() { printf "SIGNAL:%s:%s\\n" "$1" "$2"; }\n'
                      + 'stop_userspace() {' + function + '\n}\n'
                      + 'stop_userspace TERM\nstop_userspace KILL\n')
            result = subprocess.run(['sh', '-c', script], check=True, capture_output=True)
            calls = [line for line in result.stdout.decode().splitlines() if line.startswith('SIGNAL:')]
            self.assertEqual(calls, ['SIGNAL:-TERM:600', 'SIGNAL:-TERM:603',
                                     'SIGNAL:-KILL:600', 'SIGNAL:-KILL:603'])

    def test_real_offline_guard_refuses_host(self):
        run = subprocess.run(['sh', '-c', '. "$1"; offline_profile', 'guard',
                              str(ROOT / 'installer/overlay-common.sh')], capture_output=True)
        self.assertNotEqual(run.returncode, 0)

    def test_managed_scope_rejects_identity_and_symlink_escape(self):
        with tempfile.TemporaryDirectory() as tmp:
            target = Path(tmp)
            (target / 'PROGRAM').mkdir()
            (target / 'PROGRAM/revival-ui').symlink_to('/etc')
            for path in ('etc/shadow', 'opt/flashforge/serial-number', '../etc/shadow',
                         'PROGRAM/revival-ui/../secret', 'PROGRAM/revival-ui/passwd'):
                run = subprocess.run(['sh', '-c', 'ROOT=$1; . "$2"; safe_path "$3"',
                                      'guard', tmp, str(ROOT / 'installer/overlay-common.sh'), path], capture_output=True)
                self.assertNotEqual(run.returncode, 0, path)

    def test_duplicate_managed_paths_refused_even_in_conditional(self):
        with tempfile.TemporaryDirectory() as tmp:
            paths = Path(tmp) / 'paths'
            paths.write_text('PROGRAM/revival-ui/theme.conf\n' * 2)
            run = subprocess.run(['sh', '-c', '. "$1"; ROOT=$2; if validate_paths "$3"; then exit 0; else exit 9; fi',
                                  'guard', str(ROOT / 'installer/overlay-common.sh'), tmp, str(paths)], capture_output=True)
            self.assertEqual(run.returncode, 9)

    def fixture(self, base, upgrade):
        package = base / 'usb/revival'; package.mkdir(parents=True)
        storage = base / 'storage'; storage.mkdir()
        old = {}
        def save(path, data):
            full = storage / path; full.parent.mkdir(parents=True, exist_ok=True)
            full.write_bytes(data); old[path] = data
        save('PROGRAM/ffstartup.cfg', ('AppName = ./software/%VERSION%/finder-' +
             ('revival.sh' if upgrade else 'plus-arm') + '\nLogDir = /data/log\nLogLevel = 0\nLogConsole = true\n').encode())
        save('opt/flashforge/serial-number', b'fixture-serial-not-real\n')
        save('opt/flashforge/extruder-calibration', b'fixture-calibration\n')
        save('opt/flashforge/cloud-ff', b'OFF\r\n' if upgrade else b'ON\r\n')
        save('opt/flashforge/cloud-mymini', b'OFF\r\n' if upgrade else b'ON\r\n')
        save('data/user-job.gx', b'fixture-print-content-never-modified')
        if upgrade:
            for path in release.PAYLOAD_FILES:
                if path.endswith('SHA256SUMS'): continue
                save(path, ('old ' + path).encode())
            save('etc/revival-web/key', b'a' * 64)
            save('etc/revival/authorized_keys', b'fixture-owner-key-preserve')
            save('etc/revival/sshd_config', b'fixture-owner-ssh-config')
            save('PROGRAM/revival-ui/user-art.txt', b'owner-content-outside-managed-files')
        payload = {p: (('new ' + p).encode(), 0o600) for p in release.PAYLOAD_FILES if not p.endswith('SHA256SUMS')}
        for directory in ('revival-ui', 'revival-web', 'revival-logging', 'revival-network'):
            prefix = 'PROGRAM/' + directory + '/'
            sums = ''.join(hashlib.sha256(data).hexdigest() + '  ' + p[len(prefix):] + '\n'
                           for p, (data, mode) in sorted(payload.items()) if p.startswith(prefix))
            payload[prefix + 'SHA256SUMS'] = (sums.encode(), 0o600)
        (package / 'payload.tar').write_bytes(release.archive_files(payload))
        (package / 'managed-paths.txt').write_text(''.join(p+'\n' for p in sorted(release.MANAGED_FILES)))
        (package / 'payload-paths.txt').write_text(''.join(p+'\n' for p in sorted(release.PAYLOAD_FILES)))
        common = (ROOT / 'installer/overlay-common.sh').read_text()
        # Explicit fixture-only replacements: no production bypass flag or device is used.
        common += '''
offline_profile() { :; }
verify_stock() { :; }
mount() {
    if [ "$1" = -t ]; then
        for destination; do :; done
        rmdir "$destination"
        ln -s "$FIXTURE_STORAGE" "$destination"
    fi
}
umount() { :; }
e2fsck() { :; }
sync() { :; }
df() { printf 'fixture 100000 0 100000\\n'; }
'''
        (package / 'overlay-common.sh').write_text(common)
        (package / 'status-screen.sh').write_text('revival_screen() { :; }\n')
        for name in ('install-overlay.sh', 'rollback-overlay.sh'):
            text = (ROOT / 'installer' / name).read_text()
            text = text.replace('/run/revival-install', str(base / 'install-stage')).replace('/run/revival-rollback', str(base / 'rollback-stage'))
            # The fake USB is an ordinary temporary directory, never a host mount.
            text = '\n'.join(':' if line.startswith('awk -v p="$BASE" ') else line for line in text.splitlines()) + '\n'
            (package / name).write_text(text)
        sums = ''.join(hashlib.sha256(p.read_bytes()).hexdigest() + '  ' + p.name + '\n'
                       for p in sorted(package.iterdir()) if p.is_file())
        (package / 'SHA256SUMS').write_text(sums)
        for run in ('usb-fixture-install', 'usb-fixture-rollback'):
            (package / 'reports' / run).mkdir(parents=True)
        env = {**os.environ, 'FIXTURE_STORAGE': str(storage), 'REVIVAL_RUN_ID': 'usb-fixture-install'}
        run = subprocess.run(['sh', str(package/'install-overlay.sh'), '--apply'], env=env, capture_output=True)
        self.assertEqual(run.returncode, 0, run.stderr.decode())
        self.assertTrue((package / 'reports/usb-fixture-install/install/COMPLETE.txt').exists())
        self.assertIn(b'finder-revival.sh', (storage/'PROGRAM/ffstartup.cfg').read_bytes())
        return package, storage, old, env

    def rollback(self, package, env):
        return subprocess.run(['sh', str(package/'rollback-overlay.sh'), '--apply', 'usb-fixture-install'],
                              env={**env, 'REVIVAL_RUN_ID': 'usb-fixture-rollback'}, capture_output=True)

    def test_fresh_and_upgrade_round_trip(self):
        for upgrade in (False, True):
            with self.subTest(upgrade=upgrade), tempfile.TemporaryDirectory() as tmp:
                package, storage, old, env = self.fixture(Path(tmp), upgrade)
                key = (storage / 'etc/revival-web/key').read_bytes()
                run = self.rollback(package, env)
                self.assertEqual(run.returncode, 0, run.stderr.decode())
                for path, content in old.items():
                    if path.startswith('opt/flashforge/cloud-'): content = b'OFF\r\n'
                    self.assertEqual((storage/path).read_bytes(), content, path)
                for path in release.PAYLOAD_FILES - old.keys():
                    self.assertFalse((storage/path).exists(), path)
                self.assertEqual((storage/'etc/revival-web/key').read_bytes(), key)
                self.assertTrue((package/'reports/usb-fixture-rollback/rollback/RESTORED.txt').exists())

    def test_wrong_device_or_edited_file_or_corrupt_archive_refused(self):
        for issue in ('serial', 'edited', 'archive'):
            with self.subTest(issue=issue), tempfile.TemporaryDirectory() as tmp:
                package, storage, old, env = self.fixture(Path(tmp), True)
                if issue == 'serial':
                    (storage/'opt/flashforge/serial-number').write_bytes(b'another-fixture-device')
                elif issue == 'edited':
                    (storage/'PROGRAM/revival-ui/theme.conf').write_bytes(b'owner-edited-after-install')
                else:
                    archive = package/'reports/usb-fixture-install/install/rollback-private.tar'
                    archive.write_bytes(archive.read_bytes() + b'corruption')
                current = {p.relative_to(storage).as_posix(): p.read_bytes() for p in storage.rglob('*') if p.is_file()}
                run = self.rollback(package, env)
                self.assertNotEqual(run.returncode, 0)
                self.assertEqual(current, {p.relative_to(storage).as_posix(): p.read_bytes() for p in storage.rglob('*') if p.is_file()})
                self.assertFalse((package/'reports/usb-fixture-rollback/rollback').exists())


if __name__ == '__main__':
    unittest.main()
