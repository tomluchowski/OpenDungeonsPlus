#!/usr/bin/env python3
"""Measures the average frame time of a level test run for two builds and the three "Treasury detail" settings.

Both builds must contain the frame time line of the level test (the game then prints
"FRAMETIME frames=<n> avg_ms=<x>" before its PASS/FAIL line). The frame rate cap is lifted for the run
(environment variable OD_RUN_LEVEL_UNCAPPED) and vertical sync is switched off in the temporary user config,
so the numbers are the real time per frame. The game is started once per build, detail and repetition with
its own temporary user data folder; nothing is started unless a person calls this script.

Usage:
    python tools/treasury-gold/measure_frame_time.py --exe-before <old exe> --exe-after <new exe>
        [--level levels/skirmish/StoneKeep.level] [--seconds 60] [--runs 3] [--details full reduced off]
        [--config <user config.cfg to take the video settings from>]

The level should show a large, well filled treasury in view of the camera at the start (the piles only
appear for stored gold). Prints a table of the mean frame times (ms) and the change after/before.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DEFAULT_LEVEL = os.path.join(REPO, 'levels', 'skirmish', 'StoneKeep.level')
FRAME_LINE = re.compile(r'^FRAMETIME frames=(\d+) avg_ms=([0-9.eE+-]+)')


def default_config():
    appdata = os.environ.get('APPDATA')
    if appdata:
        path = os.path.join(appdata, 'OpenDungeons', 'cfg', 'config.cfg')
        if os.path.exists(path):
            return path
    return None


def set_value(text, section, key, value):
    """Sets key to value inside [section] of a user config text, adding the section or the key when missing."""
    lines = text.splitlines()
    start = None
    end = None
    for index, line in enumerate(lines):
        if line.strip() == '[' + section + ']':
            start = index
        elif start is not None and line.strip() == '[/' + section + ']':
            end = index
            break
    entry = key + '\t' + value
    if start is None or end is None:
        position = 1 if lines and lines[0].strip() == '[Configuration]' else 0
        lines[position:position] = ['[' + section + ']', entry, '[/' + section + ']']
        return '\n'.join(lines) + '\n'
    for index in range(start + 1, end):
        if lines[index].split('\t')[0] == key:
            lines[index] = entry
            return '\n'.join(lines) + '\n'
    lines.insert(end, entry)
    return '\n'.join(lines) + '\n'


def write_config(appdata, config_path, detail):
    if config_path:
        with open(config_path, encoding='utf-8') as handle:
            text = handle.read()
    else:
        text = '[Configuration]\n[/Configuration]\n'
    text = set_value(text, 'Game', 'TreasuryDetail', detail)
    text = set_value(text, 'Video', 'VSync', 'No')
    os.makedirs(os.path.join(appdata, 'cfg'), exist_ok=True)
    with open(os.path.join(appdata, 'cfg', 'config.cfg'), 'w', encoding='utf-8') as handle:
        handle.write(text)


def run_once(exe, level, seconds, detail, config_path):
    """Returns (avg_ms, frames) of one run, or None when the run failed or printed no frame time."""
    appdata = tempfile.mkdtemp(prefix='od-frame-time-')
    try:
        write_config(appdata, config_path, detail)
        environment = dict(os.environ)
        environment['OD_RUN_LEVEL_UNCAPPED'] = '1'
        command = [exe, '--run-level', level, '--seconds', str(seconds), '--appData', appdata]
        process = subprocess.run(command, cwd=os.path.dirname(os.path.abspath(exe)), env=environment,
                                 stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True,
                                 timeout=2 * seconds + 400)
        result = None
        passed = False
        for line in process.stdout.splitlines():
            match = FRAME_LINE.match(line.strip())
            if match:
                result = (float(match.group(2)), int(match.group(1)))
            if line.startswith('PASS '):
                passed = True
        if not passed or result is None:
            print('  run failed or no FRAMETIME line (%s, %s, exit code %d)' % (os.path.basename(exe), detail,
                                                                                 process.returncode))
            return None
        return result
    finally:
        shutil.rmtree(appdata, ignore_errors=True)


def mean(values):
    return sum(values) / len(values) if values else None


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--exe-before', required=True, help='build without the room batches')
    parser.add_argument('--exe-after', required=True, help='build with the room batches')
    parser.add_argument('--level', default=DEFAULT_LEVEL, help='level file with a large filled treasury')
    parser.add_argument('--seconds', type=int, default=60, help='game time per run (default 60)')
    parser.add_argument('--runs', type=int, default=3, help='runs per build and detail (default 3)')
    parser.add_argument('--details', nargs='+', default=['full', 'reduced', 'off'],
                        choices=['full', 'reduced', 'off'], help='Treasury detail settings to measure')
    parser.add_argument('--config', default=default_config(), help='user config to copy the video settings from')
    args = parser.parse_args()

    exes = [('before', os.path.abspath(args.exe_before)), ('after', os.path.abspath(args.exe_after))]
    for _, exe in exes:
        if not os.path.exists(exe):
            print('exe not found: ' + exe)
            return 2
    level = os.path.abspath(args.level)
    if not os.path.exists(level):
        print('level not found: ' + level)
        return 2

    table = {}
    for detail in args.details:
        for label, exe in exes:
            values = []
            for number in range(args.runs):
                print('%s, detail %s, run %d of %d ...' % (label, detail, number + 1, args.runs), flush=True)
                result = run_once(exe, level, args.seconds, detail, args.config)
                if result is not None:
                    values.append(result[0])
            table[(detail, label)] = mean(values)

    print()
    print('Mean frame time in ms (%d runs of %d s game time, level %s)' % (args.runs, args.seconds,
                                                                          os.path.basename(level)))
    print('%-10s %10s %10s %10s' % ('detail', 'before', 'after', 'change'))
    failed = False
    for detail in args.details:
        before = table[(detail, 'before')]
        after = table[(detail, 'after')]
        if before is None or after is None:
            print('%-10s %10s %10s %10s' % (detail, 'n/a' if before is None else '%.3f' % before,
                                           'n/a' if after is None else '%.3f' % after, 'n/a'))
            failed = True
            continue
        print('%-10s %10.3f %10.3f %+9.1f%%' % (detail, before, after, 100.0 * (after - before) / before))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
