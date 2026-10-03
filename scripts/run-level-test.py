#!/usr/bin/env python3
"""Automated level load test.

Runs the game executable once per level with the debug parameters
`--run-level <file> --seconds <N>`. The game starts a local game on the level,
lets it run for N seconds of game time, fires the win and exits with a code:

    0  PASS (level loaded, time ran, no error logged, win reported)
    1  usage error (for example --seconds 0)
    2  load error (level not found/rejected, game did not start in time)
    3  error during the run (error logged, exception, game ended without result)
    4  no victory (player seat defeated, win not reported)
    5  timeout (the watchdog of the game ended a hanging run)

Any other exit code (for example a crash) counts as FAIL "crash". The script
also kills a game that outlives its own limit. Exit code of the script: 0 only
if all levels passed.

Usage:
    python scripts/run-level-test.py --exe build/windows/Release/opendungeons-plus.exe
    python scripts/run-level-test.py --exe ... --seconds 60 levels/campaign/Mossgate.level
    python scripts/run-level-test.py --exe ... --markdown   # result paragraph for CAMPAIGN-STATE.md
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

CODE_NAMES = {
    0: "PASS",
    1: "usage error",
    2: "load error",
    3: "error during the run",
    4: "no victory",
    5: "timeout",
}


def campaign_levels():
    """Level files listed in levels/campaign/Campaign.cfg, in order."""
    cfg = os.path.join(REPO, "levels", "campaign", "Campaign.cfg")
    levels = []
    with open(cfg, encoding="utf-8") as handle:
        for line in handle:
            match = re.match(r"\s*File\s*=\s*(\S+)", line)
            if match:
                levels.append(os.path.join(REPO, "levels", match.group(1)))
    return levels


def run_level(exe, level, seconds):
    """Runs one level. Returns (passed, text)."""
    appdata = tempfile.mkdtemp(prefix="od-run-level-")
    limit = 2 * seconds + 400
    command = [exe, "--run-level", level, "--seconds", str(seconds), "--appData", appdata]
    process = subprocess.Popen(command, cwd=os.path.dirname(os.path.abspath(exe)),
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               universal_newlines=True)
    try:
        output, _ = process.communicate(timeout=limit)
        code = process.returncode
    except subprocess.TimeoutExpired:
        process.kill()
        output, _ = process.communicate()
        code = None

    line = ""
    for text in (output or "").splitlines():
        if text.startswith("PASS ") or text.startswith("FAIL "):
            line = text.strip()
    if not line:
        result_file = os.path.join(appdata, "run-level-result.txt")
        if os.path.exists(result_file):
            with open(result_file, encoding="utf-8") as handle:
                line = handle.readline().strip()
    shutil.rmtree(appdata, ignore_errors=True)

    name = os.path.basename(level)
    if code is None:
        return False, "FAIL %s : timeout: the script killed the game after %d s" % (name, limit)
    if code == 0 and line.startswith("PASS "):
        return True, line
    if code not in CODE_NAMES:
        return False, "FAIL %s : crash: exit code %d (%s)" % (name, code, line or "no result line")
    if code == 0:
        return False, "FAIL %s : exit code 0 without a PASS line" % name
    return False, line or "FAIL %s : %s (exit code %d, no result line)" % (name, CODE_NAMES[code], code)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--exe", required=True, help="built game executable")
    parser.add_argument("--seconds", type=int, default=120, help="game time per level (default 120)")
    parser.add_argument("--markdown", action="store_true", help="print the results as a CAMPAIGN-STATE.md paragraph")
    parser.add_argument("--progress-file", help="append one progress line per finished level to this file")
    parser.add_argument("levels", nargs="*", help="level files (default: all levels of Campaign.cfg)")
    args = parser.parse_args()

    exe = os.path.abspath(args.exe)
    if not os.path.exists(exe):
        print("exe not found: " + exe)
        return 2
    levels = [os.path.abspath(level) for level in args.levels] or campaign_levels()

    results = []
    for level in levels:
        passed, text = run_level(exe, level, args.seconds)
        print(text, flush=True)
        results.append((passed, text))
        pass_count = len([1 for ok, _ in results if ok])
        progress = "%d/%d Maps getestet – %d PASS, %d FAIL – zuletzt: %s %s" % (
            len(results), len(levels), pass_count, len(results) - pass_count,
            os.path.splitext(os.path.basename(level))[0], "PASS" if passed else "FAIL")
        print(progress, flush=True)
        if args.progress_file:
            with open(args.progress_file, "a", encoding="utf-8") as handle:
                handle.write(progress + "\n")

    failed = [text for passed, text in results if not passed]
    if failed:
        print("FAIL:")
        for text in failed:
            print("  " + text.splitlines()[0])
    print("%d of %d levels PASS" % (len(results) - len(failed), len(results)))
    if args.markdown:
        print()
        print("- Load test (`--run-level <file> --seconds %d`, %d levels): %d PASS, %d FAIL."
              % (args.seconds, len(results), len(results) - len(failed), len(failed)))
        for passed, text in results:
            print("  - " + text)
    return 0 if not failed else 1


if __name__ == "__main__":
    sys.exit(main())
