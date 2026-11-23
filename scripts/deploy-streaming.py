"""Deploy the latest Windows Moonlight and Sunshine builds to existing installations."""

import argparse
import base64
from contextlib import contextmanager
from dataclasses import dataclass
import getpass
import hashlib
from itertools import cycle
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from threading import Event, Thread
import time
import uuid
import zipfile

ROOT = Path(__file__).resolve().parents[1]
WORKER = ROOT / 'scripts/install-streaming-packages.ps1'
BINARIES = {
    'Moonlight': ROOT / 'build/win64/app/release/Moonlight.exe',
    'Sunshine': ROOT.parent / 'Sunshine/cmake-build-windows/sunshine.exe',
}


@dataclass(frozen=True)
class Target:
    label: str
    address: str = ''
    computer_name: str = ''
    sunshine_dir: str = r'D:\Soft\Sunshine'
    moonlight_dir: str = r'D:\Soft\moonlight'
    user: str = 'Administrator'


PROGRAM_FILES = Path(os.environ.get('ProgramFiles', r'C:\Program Files'))
TARGETS = (
    Target('本机', moonlight_dir=str(PROGRAM_FILES / 'Moonlight Game Streaming')),
    Target('CHENX', '192.168.1.28', 'CHENX', r'C:\Program Files\Sunshine', r'C:\Program Files\Moonlight Game Streaming', user='ChenX'),
    Target('CX9950X', '6.8.1.17', 'CX9950X'),
)
COMPONENTS = {
    'both': ('Moonlight', 'Sunshine'),
    'moonlight': ('Moonlight',),
    'sunshine': ('Sunshine',),
}


def digest(path):
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


def sources(args, components):
    packages = {'Moonlight': args.moonlight_package, 'Sunshine': args.sunshine_package}
    return {name: (packages[name] or BINARIES[name], packages[name] is not None) for name in components}


@contextmanager
def activity(label):
    if not sys.stdout.isatty():
        print(f'  {label}…', flush=True)
        yield
        return
    done = Event()
    started = time.monotonic()

    def animate():
        for frame in cycle('|/-\\'):
            if done.is_set():
                break
            elapsed = int(time.monotonic() - started)
            print(f'\r  {frame} {label} ({elapsed} 秒)   ', end='', flush=True)
            done.wait(0.2)

    thread = Thread(target=animate, daemon=True)
    thread.start()
    try:
        yield
    finally:
        done.set()
        thread.join()
        print(f'\r  {label} · {time.monotonic() - started:.1f} 秒    ', flush=True)


def prepare(inputs, stage):
    hashes = {}
    for name, (source, is_package) in inputs.items():
        archive = stage / f'{name}.zip'
        with activity(f'准备 {name} 安装包并计算校验值'):
            if is_package:
                shutil.copyfile(source, archive)
            else:
                with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=3) as package:
                    package.write(source, arcname=source.name)
            hashes[name] = digest(archive)
    shutil.copyfile(WORKER, stage / WORKER.name)
    return hashes


def installer_args(target, hashes, dry_run):
    values = ['-SunshineTarget', target.sunshine_dir, '-MoonlightTarget', target.moonlight_dir]
    if target.computer_name:
        values.extend(('-ExpectedComputerName', target.computer_name))
    for name, checksum in hashes.items():
        values.extend((f'-{name}Sha256', checksum))
    if dry_run:
        values.append('-StageOnly')
    return values


def ps_literal(value):
    return "'" + value.replace("'", "''") + "'"


class SkipTarget(Exception):
    pass


def authenticate(ssh, args, target):
    import paramiko

    user = args.user or target.user
    options = dict(hostname=target.address, username=user, timeout=15, banner_timeout=15, auth_timeout=15)
    account = f'{user}@{target.address}'
    if args.password_file:
        password = args.password_file.read_text(encoding='utf-8-sig').rstrip('\r\n')
    else:
        try:
            with activity(f'连接 {account}（SSH 密钥）'):
                ssh.connect(**options)
            return
        except (paramiko.AuthenticationException, paramiko.SSHException) as error:
            if not isinstance(error, paramiko.AuthenticationException) and str(error) != 'No authentication methods available':
                raise
            ssh.close()
        password = None

    while True:
        if password is None:
            if not sys.stdin.isatty():
                raise RuntimeError('需要 SSH 密码。请在终端运行菜单，或提供 --password-file。')
            password = getpass.getpass(f'{account} SSH 密码（留空跳过此主机）: ')
            if not password:
                raise SkipTarget('已跳过')
        try:
            with activity(f'验证 {account} 的 SSH 密码'):
                ssh.connect(**options, password=password, look_for_keys=False, allow_agent=False)
            return
        except paramiko.BadAuthenticationType:
            raise
        except paramiko.AuthenticationException:
            ssh.close()
            if not sys.stdin.isatty():
                raise
            print('  SSH 拒绝了这个密码，可以重新输入。', flush=True)
            password = None


def upload_packages(sftp, stage, remote, files, uploaded):
    sizes = {name: (stage / name).stat().st_size for name in files}
    total = sum(sizes.values())
    offset = 0
    interactive = sys.stdout.isatty()
    try:
        for name, size in sizes.items():
            last_update = 0.0

            def report(sent, _):
                nonlocal last_update
                now = time.monotonic()
                if interactive and (sent == size or now - last_update >= 0.15):
                    fraction = (offset + sent) / max(total, 1)
                    filled = int(20 * fraction)
                    bar = '#' * filled + '-' * (20 - filled)
                    print(f'\r  上传 [{bar}] {fraction:>4.0%}  {name:<30}', end='', flush=True)
                    last_update = now

            uploaded.append(name)
            sftp.put(str(stage / name), f'{remote}/{name}', callback=report)
            offset += size
            if not interactive:
                print(f'  已上传 {name}（{offset / max(total, 1):.0%}）', flush=True)
    finally:
        if interactive:
            print()


def deploy_remote(args, target, stage, hashes):
    import paramiko

    staging_root = 'C:/ProgramData/MoonlightDeployments'
    remote = f'{staging_root}/{stage.name}'
    files = [*(f'{name}.zip' for name in hashes), WORKER.name]
    with paramiko.SSHClient() as ssh:
        ssh.load_system_host_keys()
        authenticate(ssh, args, target)

        with ssh.open_sftp() as sftp:
            try:
                sftp.stat(staging_root)
            except FileNotFoundError:
                sftp.mkdir(staging_root)
            sftp.mkdir(remote)
            uploaded = []
            try:
                upload_packages(sftp, stage, remote, files, uploaded)

                values = installer_args(target, hashes, args.dry_run)
                parameters = []
                for index, value in enumerate(values):
                    # Each option with a value contributes two entries. -StageOnly is last.
                    parameters.append(value if index % 2 == 0 else ps_literal(value))
                command = (
                    "$ErrorActionPreference='Stop'\n"
                    f"& {ps_literal(f'{remote}/{WORKER.name}')} -Stage {ps_literal(remote)} "
                    + ' '.join(parameters)
                )
                encoded = base64.b64encode(command.encode('utf-16le')).decode()
                with activity('远端校验安装包' if args.dry_run else '远端备份、安装并验证'):
                    _, stdout, stderr = ssh.exec_command(
                        'powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand ' + encoded,
                        timeout=600,
                    )
                    output = stdout.read().decode('utf-8-sig', 'replace')
                    errors = stderr.read().decode('utf-8-sig', 'replace')
                    status = stdout.channel.recv_exit_status()
                if status:
                    raise RuntimeError(errors.strip() or output.strip() or '安装脚本执行失败')
                return json.loads(output)
            finally:
                try:
                    for name in uploaded:
                        try:
                            sftp.remove(f'{remote}/{name}')
                        except FileNotFoundError:
                            pass
                    if not sftp.listdir(remote):
                        sftp.rmdir(remote)
                except OSError as error:
                    print(f'远端暂存目录清理失败 ({remote})：{error}', file=sys.stderr)


def deploy_local(args, target, stage, hashes):
    command = [
        'powershell', '-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass',
        '-File', str(stage / WORKER.name), '-Stage', str(stage),
        *installer_args(target, hashes, args.dry_run),
    ]
    with activity('本机校验安装包' if args.dry_run else '本机备份、安装并验证'):
        result = subprocess.run(command, capture_output=True, encoding='utf-8-sig', errors='replace')
    if result.returncode:
        raise RuntimeError(result.stderr.strip() or result.stdout.strip() or f'安装退出码 {result.returncode}')
    return json.loads(result.stdout)


def select_hosts():
    while True:
        print('\n选择部署主机（可用空格或逗号多选）')
        for index, target in enumerate(TARGETS, 1):
            address = f'  {target.address}' if target.address else f'  {os.environ.get("COMPUTERNAME", "")}'
            print(f'  {index}. {target.label}{address}')
        print('  4. 全部主机\n  0. 退出')
        selected = set(input('主机 > ').replace(',', ' ').split())
        if selected == {'0'}:
            return ()
        if selected == {'4'}:
            return TARGETS
        if selected and selected <= {'1', '2', '3'}:
            return tuple(target for index, target in enumerate(TARGETS, 1) if str(index) in selected)
        print('请输入 1、2、3 中的一个或多个，或选择 4 / 0。')


def menu(args):
    print('Moonlight / Sunshine 部署')
    targets = select_hosts()
    if not targets:
        return None
    choices = {'1': 'both', '2': 'moonlight', '3': 'sunshine'}
    print('\n  1. Moonlight + Sunshine\n  2. 仅 Moonlight\n  3. 仅 Sunshine\n  0. 退出')
    while True:
        choice = input('组件 [1] > ').strip() or '1'
        if choice == '0':
            return None
        if choice in choices:
            break
        print('请输入 1、2、3 或 0。')
    inputs = sources(args, COMPONENTS[choices[choice]])
    print('\n目标：' + '、'.join(target.label for target in targets))
    for name, (path, _) in inputs.items():
        print(f'{name}：{path}')
    if args.dry_run:
        print('\n  1. 仅校验（无需重启）\n  0. 退出')
        actions = {'1'}
    else:
        print('\n  1. 执行部署（可能中断串流）\n  2. 仅校验（无需重启）\n  0. 退出')
        actions = {'1', '2'}
    while True:
        action = input('操作 [0] > ').strip() or '0'
        if action == '0':
            return None
        if action in actions:
            args.dry_run = args.dry_run or action == '2'
            return targets, inputs
        print('请选择菜单中列出的操作。')


def deploy(args, targets, inputs):
    for source, _ in inputs.values():
        if not source.is_file():
            raise FileNotFoundError(f'找不到部署文件：{source}')
    stage = ROOT / 'build' / f'deploy-streaming-{uuid.uuid4().hex}'
    stage.mkdir(parents=True)
    failed = []
    try:
        print('正在打包…', flush=True)
        hashes = prepare(inputs, stage)
        for index, target in enumerate(targets, 1):
            print(f'\n[{index}/{len(targets)}] 正在{"校验" if args.dry_run else "部署"} {target.label}…', flush=True)
            try:
                result = (deploy_remote if target.address else deploy_local)(args, target, stage, hashes)
                if args.dry_run:
                    print(f'校验通过，{result["FilesToUpdate"]} 个文件待更新。')
                else:
                    print(f'完成，已更新 {result["ChangedFiles"]} 个文件。')
                    if result.get('Service'):
                        print(f'Sunshine 服务：{result["Service"]}')
                    if result.get('Backup'):
                        print(f'备份：{result["Backup"]}')
            except SkipTarget:
                failed.append(target.label)
                print(f'已跳过 {target.label}。', flush=True)
            except Exception as error:
                # Report failures per host so a transport error does not skip later selections.
                failed.append(target.label)
                print(f'{target.label} 失败：{error}', file=sys.stderr)
    finally:
        for name in [*(f'{product}.zip' for product in inputs), WORKER.name]:
            (stage / name).unlink(missing_ok=True)
        if not any(stage.iterdir()):
            stage.rmdir()
    if failed:
        print('未完成：' + '、'.join(failed), file=sys.stderr)
    return int(bool(failed))


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tui', action='store_true', help='Open the interactive deployment menu')
    parser.add_argument('--target', choices=('both', 'local', 'remote', 'all'), default='both')
    parser.add_argument('--host', default='6.8.1.17')
    parser.add_argument('--user', help='Override the preset SSH username')
    parser.add_argument('--password-file', type=Path)
    parser.add_argument('--component', choices=COMPONENTS, default='both')
    parser.add_argument('--moonlight-package', type=Path, help='Optional full runtime ZIP')
    parser.add_argument('--sunshine-package', type=Path, help='Optional full runtime ZIP')
    parser.add_argument('--dry-run', action='store_true', help='Validate packages and report changes without installing')
    args = parser.parse_args(argv)
    try:
        if not argv or args.tui:
            selection = menu(args)
            if selection is None:
                return 0
            targets, inputs = selection
        else:
            by_address = {target.address: target for target in TARGETS if target.address}
            remote = by_address.get(args.host, Target(args.host, args.host))
            targets = {
                'local': TARGETS[:1], 'remote': (remote,), 'both': (TARGETS[0], remote), 'all': TARGETS,
            }[args.target]
            inputs = sources(args, COMPONENTS[args.component])
        return deploy(args, targets, inputs)
    except (EOFError, KeyboardInterrupt):
        print('\n已退出。')
        return 130
    except (OSError, ValueError) as error:
        print(error, file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
