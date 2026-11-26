"""Deploy the latest Windows Moonlight and Sunshine builds to existing installations."""

import argparse
import base64
from collections import deque
from concurrent.futures import Future, ThreadPoolExecutor
from contextlib import contextmanager
from dataclasses import dataclass
import getpass
import hashlib
from itertools import cycle
import json
import os
from pathlib import Path
from queue import Empty, Queue
import shutil
import stat
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


def build_components(inputs):
    for name, (binary, is_package) in inputs.items():
        if is_package:
            print(f'{name} 使用指定的 ZIP，跳过构建。', flush=True)
            continue
        env = os.environ.copy()
        if name == 'Moonlight':
            directory = ROOT / 'build/win64'
            makefile = directory / 'Makefile'
            qmake = shutil.which('qmake.exe')
            if makefile.is_file():
                for line in makefile.read_text(encoding='utf-8', errors='replace').splitlines():
                    key, _, value = line.partition('=')
                    if key.strip() == 'QMAKE':
                        cached = Path(value.strip().strip('"'))
                        if cached.is_file():
                            qmake = str(cached)
                        break
            if not qmake:
                raise FileNotFoundError('找不到 qmake.exe，请将 x64 MSVC 版 Qt 的 bin 目录加入 PATH。')
            directory.mkdir(parents=True, exist_ok=True)
            env['MOONLIGHT_QMAKE'] = qmake
            command = [str(ROOT / 'scripts/build-moonlight-release.bat')]
        else:
            directory = binary.parent
            cache = directory / 'CMakeCache.txt'
            if not cache.is_file():
                raise FileNotFoundError(f'请先配置 Sunshine 的 Release 构建目录：{directory}')
            settings = dict(line.split('=', 1) for line in cache.read_text(encoding='utf-8').splitlines()
                            if '=' in line and not line.startswith(('#', '//')))
            build_type = settings.get('CMAKE_BUILD_TYPE:STRING', '')
            if not settings.get('CMAKE_CONFIGURATION_TYPES:STRING') and build_type.lower() != 'release':
                raise ValueError(f'Sunshine 构建目录需要配置为 Release：{directory}')
            cmake = settings.get('CMAKE_COMMAND:INTERNAL', 'cmake')
            compiler = settings.get('CMAKE_CXX_COMPILER:FILEPATH')
            if compiler:
                env['PATH'] = str(Path(compiler).parent) + os.pathsep + env['PATH']
            command = [cmake, '--build', '.', '--config', 'Release', '--target', 'sunshine']
        print(f'正在构建 {name} Release…', flush=True)
        subprocess.run(command, cwd=directory, env=env, check=True)
        if not build_version(binary):
            raise FileNotFoundError(f'{name} 构建未生成部署文件：{binary}')


@contextmanager
def activity(label, report=None):
    if report or not sys.stdout.isatty():
        (report or print)(f'  {label}…', flush=True)
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


class BuildNotReady(Exception):
    pass


def build_version(path):
    try:
        info = path.stat()
    except (FileNotFoundError, PermissionError):
        return None
    if stat.S_ISREG(info.st_mode) and info.st_size:
        return info.st_mtime_ns, info.st_size, info.st_ino
    return None


def check_build(path, version):
    if build_version(path) != version:
        raise BuildNotReady()
    if sys.platform == 'win32':
        import _winapi

        try:
            # Check for an open writer without holding up the next build during compression.
            share_read = 1  # FILE_SHARE_READ
            handle = _winapi.CreateFile(str(path), _winapi.GENERIC_READ, share_read,
                                        0, _winapi.OPEN_EXISTING, 0, 0)
        except OSError as error:
            if isinstance(error, FileNotFoundError) or getattr(error, 'winerror', None) == 32:
                raise BuildNotReady() from error
            raise
        _winapi.CloseHandle(handle)


def prepare(inputs, stage, report, versions=None):
    hashes = {}
    for name, (source, is_package) in inputs.items():
        archive = stage / f'{name}.zip'
        with activity(f'准备 {name} 安装包并计算校验值', report):
            if versions is not None:
                check_build(source, versions[name])
            try:
                if is_package:
                    shutil.copyfile(source, archive)
                else:
                    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=3) as package:
                        package.write(source, arcname=source.name)
            except OSError:
                if versions is not None:
                    check_build(source, versions[name])
                raise
            if versions is not None:
                check_build(source, versions[name])
            hashes[name] = digest(archive)
    shutil.copyfile(WORKER, stage / WORKER.name)
    return hashes


def worker_args(args, target, hashes):
    values = []
    if hashes:
        values.extend(('-SunshineTarget', target.sunshine_dir, '-MoonlightTarget', target.moonlight_dir))
    if target.computer_name:
        values.extend(('-ExpectedComputerName', target.computer_name))
    for name, checksum in hashes.items():
        values.extend((f'-{name}Sha256', checksum))
    if args.keep_backups is not None:
        values.extend(('-KeepBackups', str(args.keep_backups)))
    if args.dry_run:
        values.append('-StageOnly')
    return values


def ps_literal(value):
    return "'" + value.replace("'", "''") + "'"


class PasswordRequired(Exception):
    pass


def authenticate(ssh, args, target, password, report):
    import paramiko

    user = args.user or target.user
    options = dict(hostname=target.address, username=user, timeout=15, banner_timeout=15, auth_timeout=15)
    account = f'{user}@{target.address}'
    if password is None and args.password_file:
        password = args.password_file.read_text(encoding='utf-8-sig').rstrip('\r\n')
    if password is None:
        try:
            with activity(f'连接 {account}（SSH 密钥）', report):
                ssh.connect(**options)
            return
        except (paramiko.AuthenticationException, paramiko.SSHException) as error:
            if not isinstance(error, paramiko.AuthenticationException) and str(error) != 'No authentication methods available':
                raise
            if not sys.stdin.isatty():
                raise RuntimeError('需要 SSH 密码。请在终端运行菜单，或提供 --password-file。') from error
            raise PasswordRequired() from error

    try:
        with activity(f'验证 {account} 的 SSH 密码', report):
            ssh.connect(**options, password=password, look_for_keys=False, allow_agent=False)
    except paramiko.BadAuthenticationType:
        raise
    except paramiko.AuthenticationException as error:
        if not sys.stdin.isatty():
            raise
        raise PasswordRequired('SSH 拒绝了这个密码，可以重新输入。') from error


def upload_packages(sftp, stage, remote, files, uploaded, log):
    sizes = {name: (stage / name).stat().st_size for name in files}
    total = sum(sizes.values())
    offset = 0
    interactive = not log and sys.stdout.isatty()
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
                (log or print)(f'  已上传 {name}（{offset / max(total, 1):.0%}）', flush=True)
    finally:
        if interactive:
            print()


def deploy_remote(args, target, stage, packages, report, password):
    import paramiko

    staging_root = 'C:/ProgramData/MoonlightDeployments'
    remote = f'{staging_root}/{stage.name}'
    with paramiko.SSHClient() as ssh:
        ssh.load_system_host_keys()
        authenticate(ssh, args, target, password, report)

        hashes = packages.result()
        files = [*(f'{name}.zip' for name in hashes), WORKER.name]
        with ssh.open_sftp() as sftp:
            try:
                sftp.stat(staging_root)
            except FileNotFoundError:
                sftp.mkdir(staging_root)
            sftp.mkdir(remote)
            uploaded = []
            try:
                upload_packages(sftp, stage, remote, files, uploaded, report)

                values = worker_args(args, target, hashes)
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
                label = '远端检查部署备份' if args.clean_backups else (
                    '远端校验安装包' if args.dry_run else '远端备份、安装并验证'
                )
                with activity(label, report):
                    _, stdout, stderr = ssh.exec_command(
                        'powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand ' + encoded,
                        timeout=600,
                    )
                    output = stdout.read().decode('utf-8-sig', 'replace')
                    errors = stderr.read().decode('utf-8-sig', 'replace')
                    status = stdout.channel.recv_exit_status()
                if status:
                    raise RuntimeError(errors.strip() or output.strip() or '部署脚本执行失败')
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
                except FileNotFoundError:
                    pass
                except OSError as error:
                    (report or print)(f'远端暂存目录清理失败 ({remote})：{error}', file=sys.stderr)


def deploy_local(args, target, stage, packages, report):
    hashes = packages.result()
    command = [
        'powershell', '-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass',
        '-File', str(stage / WORKER.name), '-Stage', str(stage),
        *worker_args(args, target, hashes),
    ]
    label = '本机检查部署备份' if args.clean_backups else (
        '本机校验安装包' if args.dry_run else '本机备份、安装并验证'
    )
    with activity(label, report):
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
    deploy_label = '持续部署' if args.watch else '执行部署'
    verify_label = '持续校验' if args.watch else '仅校验'
    actions = {'1': f'{verify_label}（无需重启）'} if args.dry_run else {
        '1': f'{deploy_label}（可能中断串流）', '2': f'{verify_label}（无需重启）',
    }
    if not args.watch:
        actions['3'] = '持续校验（无需重启）' if args.dry_run else '持续部署（可能中断串流）'
    if args.build:
        print('将先构建所选组件。')
    elif any(not is_package for _, is_package in inputs.values()):
        build_verify = '先构建，再持续校验' if args.watch else '先构建，再校验'
        actions['4'] = build_verify if args.dry_run else f'先构建，再{deploy_label}（可能中断串流）'
        if not args.dry_run:
            actions['5'] = build_verify
    print()
    for key, label in actions.items():
        print(f'  {key}. {label}')
    print('  0. 退出')
    while True:
        action = input('操作 [0] > ').strip() or '0'
        if action == '0':
            return None
        if action in actions:
            args.dry_run = args.dry_run or action in ('2', '5')
            args.build = args.build or action in ('4', '5')
            args.watch = args.watch or action == '3'
            return targets, inputs
        print('请选择菜单中列出的操作。')


def ask_password(args, target):
    account = f'{args.user or target.user}@{target.address}'
    return getpass.getpass(f'{account} SSH 密码（留空跳过此主机）: ')


def deploy_hosts(args, targets, stage, inputs, versions, passwords):
    workers = min(args.jobs, len(targets))
    events = Queue()
    active = {}
    failed = set()

    def package_report(message, *, flush=True):
        events.put((message, None))

    with ThreadPoolExecutor(max_workers=workers + 1) as pool:
        if inputs:
            print('正在后台打包…', flush=True)
        packages = pool.submit(prepare, inputs, stage, package_report, versions)
        pending = deque()

        def submit(target, password=None):
            def report(message, *, file=None, flush=True):
                events.put((f'[{target.label}] {message.strip()}', file))

            log = report if len(targets) > 1 else None
            if target.address:
                future = pool.submit(deploy_remote, args, target, stage, packages, log, password)
            else:
                future = pool.submit(deploy_local, args, target, stage, packages, log)
            active[future] = target
            future.add_done_callback(events.put)

        def start_next():
            if pending:
                target, password = pending.popleft()
                action = '检查备份' if args.clean_backups else ('校验' if args.dry_run else '部署')
                print(f'[{target.label}] 正在{action}…', flush=True)
                submit(target, password)

        prompt_now = sys.stdin.isatty() and not args.password_file and not args.ssh_key
        for target in targets:
            password = passwords.get(target)
            if password is None and prompt_now and target.address:
                if packages.done():
                    packages.result()
                password = ask_password(args, target)
                if not password:
                    failed.add(target)
                    print(f'[{target.label}] 已跳过。', flush=True)
                    continue
                passwords[target] = password
            pending.append((target, password))
            if len(active) < workers:
                start_next()
        while active:
            try:
                # A bounded wait lets the main thread handle Ctrl+C on Windows.
                event = events.get(timeout=0.25)
            except Empty:
                continue
            if not isinstance(event, Future):
                message, stream = event
                print(message, file=stream, flush=True)
                continue
            target = active.pop(event)
            prefix = f'[{target.label}]'
            try:
                result = event.result()
                if not args.clean_backups and args.dry_run:
                    print(f'{prefix} 校验通过，{result["FilesToUpdate"]} 个文件待更新。')
                elif not args.clean_backups:
                    print(f'{prefix} 完成，已更新 {result["ChangedFiles"]} 个文件。')
                    if result.get('Service'):
                        print(f'{prefix} Sunshine 服务：{result["Service"]}')
                    if result.get('Backup'):
                        print(f'{prefix} 备份：{result["Backup"]}')
                cleanup = result if args.clean_backups else result.get('BackupCleanup')
                if cleanup is not None:
                    label = '待删除' if args.dry_run else '已删除'
                    backups = cleanup['Backups']
                    print(f'{prefix} 保留 {cleanup["Kept"]} 份备份，{label} {len(backups)} 份。')
                    if args.dry_run:
                        for backup in backups:
                            print(f'{prefix}   {backup}')
                if result.get('BackupCleanupError'):
                    failed.add(target)
                    print(f'{prefix} 清理备份失败：{result["BackupCleanupError"]}', file=sys.stderr, flush=True)
            except PasswordRequired as error:
                passwords.pop(target, None)
                if str(error):
                    print(f'{prefix} {error}', flush=True)
                password = ask_password(args, target)
                if password:
                    passwords[target] = password
                    submit(target, password)
                    continue
                failed.add(target)
                print(f'{prefix} 已跳过。', flush=True)
            except BuildNotReady:
                raise
            except Exception as error:
                failed.add(target)
                print(f'{prefix} 失败：{error}', file=sys.stderr, flush=True)
            start_next()
        if versions is not None:
            packages.result()
    if failed:
        print('未完成：' + '、'.join(t.label for t in targets if t in failed), file=sys.stderr)
    return int(bool(failed))


def deploy(args, targets, inputs, *, versions=None, passwords=None):
    if versions is None:
        for source, _ in inputs.values():
            if not source.is_file():
                raise FileNotFoundError(f'找不到部署文件：{source}')
    stage = ROOT / 'build' / f'deploy-streaming-{uuid.uuid4().hex}'
    stage.mkdir(parents=True)
    try:
        return deploy_hosts(args, targets, stage, inputs, versions,
                            {} if passwords is None else passwords)
    finally:
        for name in [*(f'{product}.zip' for product in inputs), WORKER.name]:
            (stage / name).unlink(missing_ok=True)
        try:
            if not any(stage.iterdir()):
                stage.rmdir()
        except FileNotFoundError:
            pass


def watch(args, targets, inputs):
    print('持续监听构建（按 Ctrl+C 退出）', flush=True)
    for name, (path, _) in inputs.items():
        print(f'  {name}：{path}', flush=True)
    passwords = {}
    attempted = {}
    observed = {}
    changed_at = {}
    while True:
        time.sleep(0.5)
        now = time.monotonic()
        ready = {}
        for name, (path, _) in inputs.items():
            version = build_version(path)
            if version != observed.get(name):
                observed[name] = version
                changed_at[name] = now
            elif version is not None and version != attempted.get(name) and now - changed_at[name] >= 1:
                try:
                    check_build(path, version)
                except BuildNotReady:
                    continue
                ready[name] = version
        if ready:
            action = '校验' if args.dry_run else '部署'
            print(f'构建已就绪，开始{action}：' + '、'.join(ready), flush=True)
            try:
                deploy(args, targets, {name: inputs[name] for name in ready},
                       versions=ready, passwords=passwords)
            except BuildNotReady:
                continue
            except (OSError, ValueError) as error:
                print(error, file=sys.stderr, flush=True)
            attempted.update(ready)


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tui', action='store_true', help='Open the interactive deployment menu')
    parser.add_argument('--build', action='store_true', help='Build selected Release binaries once before deployment, verification or watch')
    parser.add_argument('--watch', action='store_true', help='Deploy available builds and redeploy changed outputs until Ctrl+C')
    parser.add_argument('--clean-backups', action='store_true', help='Clean backups once, or after each successful deployment with --watch')
    parser.add_argument('--keep-backups', type=int, help='Enable cleanup and keep this many newest backups per host (--clean-backups defaults to 5)')
    parser.add_argument('--target', choices=('both', 'local', 'remote', 'all'), default='both')
    parser.add_argument('-j', '--jobs', type=int, default=3, help='Maximum concurrent hosts (default: 3, use 1 for serial deployment)')
    parser.add_argument('--host', default='6.8.1.17')
    parser.add_argument('--user', help='Override the preset SSH username')
    auth = parser.add_mutually_exclusive_group()
    auth.add_argument('--password-file', type=Path)
    auth.add_argument('--ssh-key', action='store_true', help='Try SSH keys before prompting for a password')
    parser.add_argument('--component', choices=COMPONENTS, default='both')
    parser.add_argument('--moonlight-package', type=Path, help='Optional full runtime ZIP')
    parser.add_argument('--sunshine-package', type=Path, help='Optional full runtime ZIP')
    parser.add_argument('--dry-run', action='store_true', help='Validate packages or preview backup cleanup without making changes')
    args = parser.parse_args(argv)
    if args.jobs < 1:
        parser.error('--jobs must be at least 1')
    if args.clean_backups and args.keep_backups is None:
        args.keep_backups = 5
    if args.keep_backups is not None and not 0 <= args.keep_backups <= 2147483647:
        parser.error('--keep-backups must be between 0 and 2147483647')
    args.clean_backups = args.clean_backups and not args.watch
    if args.clean_backups and args.build:
        parser.error('--build cannot be used with standalone backup cleanup')
    try:
        if args.clean_backups and (args.tui or argv == ['--clean-backups']):
            targets = select_hosts()
            if not targets:
                return 0
            inputs = {}
        elif not argv or argv == ['--watch'] or args.tui:
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
            inputs = {} if args.clean_backups else sources(args, COMPONENTS[args.component])
        if args.build:
            build_components(inputs)
        if args.watch:
            return watch(args, targets, inputs)
        return deploy(args, targets, inputs)
    except (EOFError, KeyboardInterrupt):
        print('\n已退出。')
        return 130
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(error, file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
