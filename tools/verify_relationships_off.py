#!/usr/bin/env python3
"""Checks that the creature relationships option off leaves the game without relationship traces.

Runs one level headless with the debug parameters `--run-level <file> --seconds <N> --seed <S>`
with the level file switched to "Relationships Off". The run must pass, the game must write a
non-empty state (seats and creatures, run-level-state.txt), and neither the program output nor
any log file of the run may mention relationship values, events, tiers or bonuses. Allowed are
only the start-up lines about loading the relationships data file (and its missing-entries warning).

Exit code 0: no relationship traces. 1: traces found or the state is empty. 2: the run failed.

Usage:
    python tools/verify_relationships_off.py --exe CURRENT.exe
        [--level levels/skirmish/DuelToDeath.level] [--seconds 60] [--seed 12345]
"""

import argparse
import glob
import os
import shutil
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ALLOWED = ("relationships.cfg: missing or invalid entries", "Load relationships file")


def switch_option_off(level, target):
    """Copies the level and adds the line `Relationships<TAB>Off` at the end of its [Info] block."""
    with open(level, encoding="utf-8", newline="") as handle:
        text = handle.read()
    newline = "\r\n" if "\r\n" in text else "\n"
    marker = "[/Info]"
    if marker not in text:
        raise SystemExit("level has no [Info] block: " + level)
    text = text.replace(marker, "Relationships\tOff" + newline + marker, 1)
    with open(target, "w", encoding="utf-8", newline="") as handle:
        handle.write(text)


def run(exe, level, seconds, seed):
    """Runs the game once. Returns (result line, state text, text lines to scan)."""
    appdata = tempfile.mkdtemp(prefix="od-verify-")
    command = [exe, "--run-level", level, "--seconds", str(seconds), "--seed", str(seed), "--appData", appdata]
    limit = 2 * seconds + 400
    process = subprocess.Popen(command, cwd=os.path.dirname(os.path.abspath(exe)),
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
    try:
        output, _ = process.communicate(timeout=limit)
    except subprocess.TimeoutExpired:
        process.kill()
        process.communicate()
        shutil.rmtree(appdata, ignore_errors=True)
        raise SystemExit("run failed: timeout (%s)" % exe)
    scan = (output or "").splitlines()
    for path in glob.glob(os.path.join(appdata, "**", "*.log"), recursive=True):
        with open(path, encoding="utf-8", errors="replace") as handle:
            scan.extend(handle.read().splitlines())
    result = [line for line in (output or "").splitlines() if line.startswith(("PASS ", "FAIL "))]
    state_file = os.path.join(appdata, "run-level-state.txt")
    state = ""
    if os.path.exists(state_file):
        with open(state_file, encoding="utf-8") as handle:
            state = handle.read()
    shutil.rmtree(appdata, ignore_errors=True)
    if process.returncode != 0 or not result:
        raise SystemExit("run failed (exit %s): %s" % (process.returncode, result[-1] if result else "no result line"))
    return result[-1], state, scan


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--exe", required=True, help="current build (option switched off in the level)")
    parser.add_argument("--level", default=os.path.join(REPO, "levels", "skirmish", "DuelToDeath.level"))
    parser.add_argument("--seconds", type=int, default=60)
    parser.add_argument("--seed", type=int, default=12345)
    args = parser.parse_args()

    level = os.path.abspath(args.level)
    exe = os.path.abspath(args.exe)
    for path in (level, exe):
        if not os.path.exists(path):
            print("not found: " + path)
            return 2

    workdir = tempfile.mkdtemp(prefix="od-verify-level-")
    off_level = os.path.join(workdir, os.path.basename(level))
    switch_option_off(level, off_level)
    try:
        result, state, scan = run(exe, off_level, args.seconds, args.seed)
        print("run: " + result)
    except SystemExit as error:
        print(error)
        return 2
    finally:
        shutil.rmtree(workdir, ignore_errors=True)

    creatures = len([1 for line in state.splitlines() if line.startswith("creature ")])
    if not state or creatures == 0:
        print("FAIL: the run wrote no creatures to the state file")
        return 1
    traces = [line for line in scan if "relationship" in line.lower() and not any(text in line for text in ALLOWED)]
    if traces:
        print("\n".join(traces[:30]))
        print("FAIL: %d relationship lines in the output or logs with the option off" % len(traces))
        return 1
    print("OK: option off, no relationship traces (%d creatures)" % creatures)
    return 0


if __name__ == "__main__":
    sys.exit(main())
