"""Integrator-only campaign load/goal-registration probes using an already built game.

Run with --executable <game> --output <worktree output directory> while the game
is closed. This exercises the existing timed win probe, not natural playability.
"""
import argparse
import csv
import ctypes
import os
from pathlib import Path
import re
import subprocess
import sys


# Inherit the integrator runner's dependency paths and disable modal loader errors.
def probe_environment():
    if os.name != 'nt':
        raise RuntimeError('Campaign load probes require the Windows integration host')
    ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002 | 0x8000)
    environment = dict(os.environ)
    dependency_root = Path.home() / 'od-deps'
    dll_paths = [dependency_root / 'install/bin', dependency_root / 'build/ogre/bin/release']
    environment['PATH'] = os.pathsep.join([str(path) for path in dll_paths]
                                          + [environment.get('PATH', '')])
    return environment


# Read only real campaign entries, ignoring commented examples.
def campaign_levels(repo):
    text = (repo / 'levels/campaign/Campaign.cfg').read_text(encoding='utf-8')
    active = False
    levels = []
    for raw in text.splitlines():
        line = raw.strip()
        if line == '[Level]':
            active = True
        elif line == '[/Level]':
            active = False
        elif active and line.startswith('File='):
            levels.append(repo / 'levels' / line.split('=', 1)[1].strip())
    return levels


# Refuse to launch probes while another copy of this game is running.
def game_is_running(executable):
    if os.name != 'nt':
        raise RuntimeError('Game process detection requires the Windows integration host')
    result = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq ' + executable.name,
                             '/FO', 'CSV', '/NH'], stdin=subprocess.DEVNULL,
                            capture_output=True, text=True, timeout=30, check=True)
    return any(row and row[0].casefold() == executable.name.casefold()
               for row in csv.reader(result.stdout.splitlines()))


# Run every configured map once, retaining logs and real process exit codes.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--repo', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--timeout', type=int, default=180)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    repo = args.repo.resolve(strict=True)
    levels = campaign_levels(repo)
    if not levels:
        parser.error('No configured campaign maps found')
    args.output.mkdir(parents=True, exist_ok=True)
    environment = probe_environment()
    failed = 0
    for level in levels:
        if game_is_running(executable):
            raise RuntimeError('Close the running game before campaign load probes')
        command = [str(executable), '--run-level', str(level), '--seconds', '1']
        try:
            result = subprocess.run(command, cwd=executable.parent,
                                    stdin=subprocess.DEVNULL, capture_output=True,
                                    timeout=args.timeout, env=environment)
            log = (result.stdout + result.stderr).decode('utf-8', errors='replace')
            passed = result.returncode == 0 and re.search(r'GOALS registered=[1-9][0-9]*', log)
            status = 'PASS' if passed else 'FAIL exit=' + str(result.returncode)
        except subprocess.TimeoutExpired as error:
            log = ((error.stdout or b'') + (error.stderr or b'')).decode('utf-8', errors='replace')
            passed = False
            status = 'FAIL timeout'
        (args.output / (level.stem + '.log')).write_text(log, encoding='utf-8')
        print(level.name + ': ' + status, flush=True)
        if not passed:
            failed += 1
    print('Campaign load probes: total=%d failed=%d' % (len(levels), failed))
    return int(failed != 0)


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
