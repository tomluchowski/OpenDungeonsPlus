#!/usr/bin/env python3
"""Check that every sound has a source and a free licence in CREDITS (no compiler needed).

    python scripts/check_sound_sources.py
    python scripts/check_sound_sources.py --self-test

Rules:
- every audio file under sounds/ and music/ is covered by a CREDITS entry, and every audio entry of CREDITS
  points to at least one existing file;
- every entry for sounds/ has a source URL and the licence CC0, CC-BY or CC-BY-SA;
- no entry for a sound names synthesised, generated or a script as its origin;
- music/ is only checked for the CREDITS entry.
A few older real recordings are listed below with the reason why they are accepted differently.
"""

import argparse
import fnmatch
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
AUDIO = (".ogg", ".wav", ".flac", ".mp3")

# Own recordings of a former project member; accepted without a source URL, only with this exact text
OWN_RECORDING_TEXT = "own recordings of a former OpenDungeons member, no public source"
OWN_RECORDING_FILES = ("SwordBlock*.ogg", "Digging*.ogg", "default_build_trap.ogg", "Claim01.ogg",
                       "RocksFalling*.ogg")

# Older real recordings under a GPL licence: accepted without a source URL (existing recordings, owner
# decision): the interface click and the keeper voice spoken by a project member.
GPL_FILES = ("click.ogg", "OD_voice_keeper(neutral)_*.ogg")

# Real recordings in the public domain, taken from Wikimedia Commons (hatchery chickens)
PUBLIC_DOMAIN_FOLDER = "Spatial/Rooms/Hatchery/"

LICENSE_RE = re.compile(r"CC0|CC-BY-SA\s*[0-9.]+|CC-BY\s*[0-9.]+|Public domain|GPL\w*\s*[0-9.]*\+?")
ENTRY_RE = re.compile(r"^(->\s*)?(\S*?\.(?:ogg|wav|flac|mp3))(.*)$")
URL_RE = re.compile(r"https?://\S+")
FORBIDDEN_RE = re.compile(r"synthesi[sz]ed|generated|\bscript\b", re.I)


def expand_braces(text):
    match = re.search(r"\{([^{}]*)\}", text)
    if match is None:
        return [text]
    result = []
    for part in match.group(1).split(","):
        result.extend(expand_braces(text[:match.start()] + part + text[match.end():]))
    return result


def parse_credits(text):
    """Returns a list of (base, pattern, rest) for every audio entry; base is sounds or music"""
    entries = []
    base = "sounds"
    current = ""
    for raw in text.splitlines():
        line = raw.strip()
        if not line:
            continue
        if line.startswith("=="):
            if re.search(r"[A-Za-z]", line):
                base = "music" if "music/" in line else "sounds"
                current = ""
            continue
        match = ENTRY_RE.match(line)
        if match is None:
            if not line.startswith("->") and "/" in line and len(line.split()) == 1:
                current = line.replace("\\", "/").rstrip("/")
            continue
        name = match.group(2).replace("\\", "/")
        rest = match.group(3)
        entry_base = base
        if name.startswith("sounds/"):
            name = name[len("sounds/"):]
            entry_base = "sounds"
        elif match.group(1) is not None and base == "sounds" and current:
            name = current + "/" + name
        for pattern in expand_braces(name):
            entries.append((entry_base, pattern, rest))
    return entries


def list_files(root):
    files = {"sounds": [], "music": []}
    for base in files:
        folder = os.path.join(root, base)
        for directory, _, names in os.walk(folder):
            for name in names:
                if name.lower().endswith(AUDIO):
                    path = os.path.join(directory, name)
                    files[base].append(os.path.relpath(path, folder).replace("\\", "/"))
    return files


def matches(base, pattern, path):
    # file names differ in case between CREDITS and disk (Die*.ogg, die1.ogg)
    if base == "music":
        return fnmatch.fnmatchcase(os.path.basename(path).lower(), pattern.lower())
    return fnmatch.fnmatchcase(path.lower(), pattern.lower())


def check_entry(pattern, rest):
    """Problems of one sounds/ entry"""
    problems = []
    name = os.path.basename(pattern)
    if FORBIDDEN_RE.search(rest.replace("not AI generated", "")):
        problems.append("names a synthesised/generated/script origin: " + rest.strip())
    found = LICENSE_RE.search(rest)
    license_name = found.group(0) if found is not None else ""
    has_url = URL_RE.search(rest) is not None
    if any(fnmatch.fnmatchcase(name, p) for p in OWN_RECORDING_FILES) and "Svenskmand" in rest:
        if OWN_RECORDING_TEXT not in rest:
            problems.append("needs the text '%s' instead of a source URL" % OWN_RECORDING_TEXT)
        if not license_name.startswith("CC-BY-SA"):
            problems.append("licence is not CC-BY-SA")
        return problems
    if license_name.startswith("GPL"):
        if not any(fnmatch.fnmatchcase(name, p) for p in GPL_FILES):
            problems.append("licence %s is only accepted for the listed older recordings" % license_name)
        return problems
    if license_name == "Public domain":
        if not pattern.startswith(PUBLIC_DOMAIN_FOLDER):
            problems.append("public domain is only accepted for the hatchery recordings")
    elif not (license_name.startswith("CC0") or license_name.startswith("CC-BY")):
        problems.append("licence is not CC0, CC-BY or CC-BY-SA")
    if not has_url:
        problems.append("no source URL")
    return problems


def run(credits_text, files):
    problems = []
    entries = parse_credits(credits_text)
    covered = set()
    for base, pattern, rest in entries:
        hits = [p for p in files[base] if matches(base, pattern, p)]
        if not hits:
            problems.append("CREDITS entry %s/%s points to no existing file" % (base, pattern))
            continue
        covered.update((base, p) for p in hits)
        if base == "sounds":
            for problem in check_entry(pattern, rest):
                problems.append("CREDITS entry %s: %s" % (pattern, problem))
    for base in files:
        for path in files[base]:
            if (base, path) not in covered:
                problems.append("%s/%s has no CREDITS entry" % (base, path))
    return problems


def self_test():
    credits = "\n".join([
        "==  Music: music/ ==",
        "theme.ogg                              Someone  CC-BY 3.0  http://example.org/a",
        "==  Sounds: sounds/  ==",
        "Spatial/A/",
        "-> good*.ogg    rubberduck  CC0   https://example.org/x",
        "-> Claim01.ogg  Svenskmand  CC-BY-SA 3.0  " + OWN_RECORDING_TEXT,
        "-> bad_lic.ogg  someone  CC-BY-NC 3.0  https://example.org/y",
        "-> no_url.ogg   someone  CC0",
        "-> synth.ogg    OD Team  CC0  https://example.org/z Synthesised by tools/x.py",
        "-> ghost.ogg    someone  CC0  https://example.org/g",
        "Spatial/B/",
        "-> {x,y}.ogg    someone  CC-BY 4.0  https://example.org/b",
    ])
    files = {"sounds": ["Spatial/A/good1.ogg", "Spatial/A/Claim01.ogg", "Spatial/A/bad_lic.ogg",
                        "Spatial/A/no_url.ogg", "Spatial/A/synth.ogg", "Spatial/A/orphan.ogg",
                        "Spatial/B/x.ogg", "Spatial/B/y.ogg"],
             "music": ["theme.ogg"]}
    found = "\n".join(run(credits, files))
    expected = ["bad_lic.ogg: licence is not", "no_url.ogg: no source URL", "synth.ogg: names a synthesised",
                "points to no existing file", "Spatial/A/orphan.ogg has no CREDITS entry"]
    for text in expected:
        if text not in found:
            print("self-test: missing finding: " + text)
            return 1
    if "good1" in found or "Claim01" in found or "x.ogg" in found or "theme" in found:
        print("self-test: unexpected finding:\n" + found)
        return 1
    print("self-test: ok")
    return 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        return self_test()
    with open(os.path.join(ROOT, "CREDITS"), encoding="utf-8") as handle:
        credits = handle.read()
    files = list_files(ROOT)
    problems = run(credits, files)
    for problem in problems:
        print("FAIL: " + problem)
    print("%d sounds, %d music files, %d problems" % (len(files["sounds"]), len(files["music"]), len(problems)))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
