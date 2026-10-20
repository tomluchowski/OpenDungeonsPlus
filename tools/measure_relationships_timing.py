#!/usr/bin/env python3
"""Frame and turn time with the creature relationships option on and off.

Runs the same level headless several times with `--run-level <file> --seconds <N>`, alternating
the option off (level file with `Relationships Off`) and on (original level). The game writes
frame time statistics (client) and turn processing time (server) to run-level-timing.txt. The
script prints average and 95th percentile of both settings, the difference, and the spread
between repeated runs of the same setting (the measurement noise).

Exit code 0: the difference of the average is within the noise or below --tolerance percent.
1: it is larger. 2: a run failed.

Usage:
    python tools/measure_relationships_timing.py --exe build/windows/Release/opendungeons-plus.exe
        [--level levels/skirmish/DuelToDeath.level] [--seconds 60] [--runs 3] [--tolerance 5]
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def switch_option_off(level, target):
    """Copies the level and adds the line `Relationships<TAB>Off` at the end of its [Info] block."""
    with open(level, encoding="utf-8", newline="") as handle:
        text = handle.read()
    newline = "\r\n" if "\r\n" in text else "\n"
    marker = "[/Info]"
    if marker not in text:
        raise SystemExit("level has no [Info] block: " + level)
    with open(target, "w", encoding="utf-8", newline="") as handle:
        handle.write(text.replace(marker, "Relationships\tOff" + newline + marker, 1))


def run(exe, level, seconds):
    """Runs the game once. Returns {"frame": (avg, p95), "turn": (avg, p95)} in milliseconds."""
    appdata = tempfile.mkdtemp(prefix="od-timing-")
    command = [exe, "--run-level", level, "--seconds", str(seconds), "--appData", appdata]
    process = subprocess.Popen(command, cwd=os.path.dirname(os.path.abspath(exe)),
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
    try:
        output, _ = process.communicate(timeout=2 * seconds + 400)
    except subprocess.TimeoutExpired:
        process.kill()
        process.communicate()
        shutil.rmtree(appdata, ignore_errors=True)
        raise SystemExit("run failed: timeout")
    timing_file = os.path.join(appdata, "run-level-timing.txt")
    text = ""
    if os.path.exists(timing_file):
        with open(timing_file, encoding="utf-8") as handle:
            text = handle.read()
    shutil.rmtree(appdata, ignore_errors=True)
    if process.returncode != 0 or not text:
        lines = [line for line in (output or "").splitlines() if line.startswith(("PASS ", "FAIL "))]
        raise SystemExit("run failed (exit %s): %s" % (process.returncode, lines[-1] if lines else "no result line"))
    values = {}
    for key in ("frame_ms", "turn_ms"):
        match = re.search(r"%s n=(\d+) avg=([\d.]+) p95=([\d.]+)" % key, text)
        if not match or int(match.group(1)) == 0:
            raise SystemExit("no %s statistics in the timing file" % key)
        values[key[:-3]] = (float(match.group(2)), float(match.group(3)))
    return values


def mean(values):
    return sum(values) / len(values)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--exe", required=True, help="current build")
    parser.add_argument("--level", default=os.path.join(REPO, "levels", "skirmish", "DuelToDeath.level"))
    parser.add_argument("--seconds", type=int, default=60)
    parser.add_argument("--runs", type=int, default=3, help="runs per setting (default 3)")
    parser.add_argument("--tolerance", type=float, default=5.0, help="accepted difference of the averages in percent")
    args = parser.parse_args()

    exe = os.path.abspath(args.exe)
    level = os.path.abspath(args.level)
    for path in (exe, level):
        if not os.path.exists(path):
            print("not found: " + path)
            return 2

    workdir = tempfile.mkdtemp(prefix="od-timing-level-")
    off_level = os.path.join(workdir, os.path.basename(level))
    switch_option_off(level, off_level)
    results = {"on": [], "off": []}
    try:
        for index in range(args.runs):
            for setting, path in (("off", off_level), ("on", level)):
                values = run(exe, path, args.seconds)
                results[setting].append(values)
                print("run %d option %-3s frame avg %.3f ms p95 %.3f ms | turn avg %.3f ms p95 %.3f ms" % (
                    index + 1, setting, values["frame"][0], values["frame"][1], values["turn"][0], values["turn"][1]),
                    flush=True)
    except SystemExit as error:
        print(error)
        return 2
    finally:
        shutil.rmtree(workdir, ignore_errors=True)

    failed = False
    for kind in ("frame", "turn"):
        for position, name in ((0, "average"), (1, "p95")):
            on = [item[kind][position] for item in results["on"]]
            off = [item[kind][position] for item in results["off"]]
            diff = mean(on) - mean(off)
            percent = 100.0 * diff / mean(off) if mean(off) > 0 else 0.0
            noise = max(max(on) - min(on), max(off) - min(off))
            print("%s %s: off %.3f ms, on %.3f ms, difference %+.3f ms (%+.1f %%), spread between runs %.3f ms" % (
                kind, name, mean(off), mean(on), diff, percent, noise))
            if name == "average" and abs(diff) > noise and abs(percent) > args.tolerance:
                failed = True
    print("FAIL: the option changes the average beyond the noise" if failed else "OK: difference within noise")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
