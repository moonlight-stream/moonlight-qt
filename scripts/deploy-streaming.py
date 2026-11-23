"""Update the existing Moonlight and Sunshine installations on both Windows PCs."""
import argparse
import base64
import getpass
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import uuid
import zipfile

ROOT = Path(__file__).resolve().parents[1]
WORKER = ROOT / 'scripts/install-streaming-packages.ps1'


def digest(path):
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


def prepare(args, stage):
    moon = stage / 'Moonlight.zip'
    if args.moonlight_package:
        shutil.copyfile(args.moonlight_package, moon)
    else:
        template = ROOT / 'build/Moonlight-BidirectionalClipboard-x64.zip'
        exe = ROOT / 'build/win64/app/release/Moonlight.exe'
        replaced = 0
        with zipfile.ZipFile(template) as src, zipfile.ZipFile(moon, 'w') as dest:
            for info in src.infolist():
                if info.filename.removeprefix('./').lower() == 'moonlight.exe':
                    dest.write(exe, info.filename, compress_type=zipfile.ZIP_DEFLATED)
                    replaced += 1
                else:
                    dest.writestr(info, src.read(info))
        if replaced != 1:
            raise ValueError('Missing or ambiguous Moonlight.exe in the runtime package')
    sun = stage / 'Sunshine.zip'
    shutil.copyfile(args.sunshine_package, sun)
    shutil.copyfile(WORKER, stage / WORKER.name)
    return ['-SunshineSha256', digest(sun), '-MoonlightSha256', digest(moon)]


def deploy_remote(args, stage, hashes):
    import paramiko

    remote = 'D:/Soft/' + stage.name
    with paramiko.SSHClient() as ssh:
        ssh.load_system_host_keys(str(Path.home() / '.ssh/known_hosts'))
        options = dict(hostname=args.host, username=args.user, timeout=15)
        if args.password_file:
            password = args.password_file.read_text(encoding='utf-8-sig').rstrip('\r\n')
            ssh.connect(**options, password=password, look_for_keys=False, allow_agent=False)
        else:
            try:
                ssh.connect(**options)
            except (paramiko.AuthenticationException, paramiko.SSHException) as error:
                if not isinstance(error, paramiko.AuthenticationException) and str(error) != 'No authentication methods available':
                    raise
                ssh.connect(**options, password=getpass.getpass(f'{args.user}@{args.host} SSH password: '),
                            look_for_keys=False, allow_agent=False)
        with ssh.open_sftp() as sftp:
            sftp.mkdir(remote)
            for name in ('Moonlight.zip', 'Sunshine.zip', WORKER.name):
                sftp.put(str(stage / name), remote + '/' + name)
        command = "& '" + remote + "/" + WORKER.name + "' -Stage '" + remote + "' " + ' '.join(hashes)
        encoded = base64.b64encode(("$ErrorActionPreference='Stop'\n" + command).encode('utf-16le')).decode()
        _, stdout, stderr = ssh.exec_command('powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand ' + encoded, timeout=600)
        output = stdout.read().decode('utf-8-sig', 'replace')
        errors = stderr.read().decode('utf-8', 'replace')
        if stdout.channel.recv_exit_status():
            raise RuntimeError(errors or output)
        result = json.loads(output)
        with ssh.open_sftp() as sftp:
            for name in ('Moonlight.zip', 'Sunshine.zip', WORKER.name):
                sftp.remove(remote + '/' + name)
            if not result['Backup']:
                sftp.rmdir(remote)
        return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', choices=('both', 'local', 'remote'), default='both')
    parser.add_argument('--host', default='6.8.1.17')
    parser.add_argument('--user', default='Administrator')
    parser.add_argument('--password-file', type=Path)
    parser.add_argument('--moonlight-package', type=Path, help='Complete runtime ZIP, used verbatim')
    parser.add_argument('--sunshine-package', type=Path, default=ROOT.parent /
                        'Sunshine/cmake-build-windows/Sunshine-BidirectionalClipboard-x64.zip')
    args = parser.parse_args()
    stage = ROOT / 'build' / ('deploy-streaming-' + uuid.uuid4().hex)
    stage.mkdir(parents=True)
    hashes = prepare(args, stage)
    if args.target in ('local', 'both'):
        cmd = ['powershell', '-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File',
               str(WORKER), '-Stage', str(stage), '-MoonlightTarget',
               str(Path(os.environ['ProgramFiles']) / 'Moonlight Game Streaming'), *hashes]
        subprocess.run(cmd, check=True)
    if args.target in ('remote', 'both'):
        print(json.dumps(deploy_remote(args, stage, hashes), indent=2))
    for name in ('Moonlight.zip', 'Sunshine.zip', WORKER.name):
        (stage / name).unlink()
    if not any(stage.iterdir()):
        stage.rmdir()


if __name__ == '__main__':
    main()
