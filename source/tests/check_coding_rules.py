#!/usr/bin/env python3
"""Non-compiling check of the coding rules for the lines a change ADDS.

Only added lines are checked (diff against a base, default origin/integration/all, plus
untracked files); the existing code is never reported. Rules:

  auto            `auto` used as a type in C++ files (source/**, *.cpp/*.h) and in the C++
                  probes of check scripts.
  lambda          Lambdas and std::function in new C++. A block that a check script cuts
                  out of the source may carry the marker comment "// allowed-lambda: <why>"
                  on the same line or on the added line directly above.
  control-char    Control characters 0x00-0x08, 0x0B, 0x0C, 0x0E-0x1F in any added line.
  credits         New or changed images, models, sounds and textures without an entry in
                  CREDITS (exact path, directory, or a wildcard that matches the file; a
                  bare file name entry counts as well).
  protected-term  A word of the local term list that the pre-push hook blocks (read through
                  the same configuration as scripts/check-protected-content.py). The word
                  itself is never printed. Skipped, with a notice, if the list is missing.

Usage: python -I source/tests/check_coding_rules.py [--base <ref>]
Output: one line "<file>:<line>: <rule>: <message>" per violation. Exit 0 only if there is
none. Nothing is compiled.
"""
import argparse
import importlib.util
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SELF_PATH = "source/tests/check_coding_rules.py"
DEFAULT_BASE = "origin/integration/all"
HOOK_SCRIPT = os.path.join("scripts", "check-protected-content.py")

# Marker that excuses a lambda / std::function in a block a check script cuts out.
LAMBDA_MARKER = "allowed-lambda:"

CPP_EXTENSIONS = (".cpp", ".h", ".hpp", ".cc", ".c")
# Check scripts hold C++ probes as text; their added lines get the C++ rules as well.
PROBE_SCRIPT_DIRS = ("source/tests/", "scripts/")

ASSET_EXTENSIONS = (".png", ".jpg", ".jpeg", ".gif", ".bmp", ".tga", ".dds", ".ico",
                    ".svg", ".mesh", ".skeleton", ".blend", ".fbx", ".obj", ".dae",
                    ".ogg", ".wav", ".mp3", ".flac", ".ttf", ".otf")
CREDITS_FILE_NAMES = ("CREDITS", "CREDITS.md", "CREDITS.txt")

# 0x00-0x08, 0x0B, 0x0C, 0x0E-0x1F.
CONTROL_CHAR_PATTERN = re.compile("[\x00-\x08\x0b\x0c\x0e-\x1f]")

# `auto` followed by something that declares a name (auto x, auto& x, auto* x, auto&& x),
# also as the head of a range-for or of a structured binding.
AUTO_PATTERN = re.compile(r"(?<![\w:])auto\b\s*(?:const\s*)?(?:[&*]{1,2}\s*)?(?:\[|\w)")

# [] / [&] / [=] style lambdas, and [x](args) lambdas. The lookbehind rules out
# subscripts such as a[i](x) and array declarations.
LAMBDA_PATTERNS = (
    re.compile(r"(?<![\w\)\]])\[\s*(?:[&=][^\]]*)?\]\s*(?:\([^)]*\)\s*)?(?:mutable\s*)?"
               r"(?:->[^{;]*)?\{"),
    re.compile(r"(?<![\w\)\]])\[[^\]\[]*\]\s*\([^)]*\)\s*(?:mutable\s*)?(?:->[^{;]*)?\{"),
)
STD_FUNCTION_PATTERN = re.compile(r"std::function\s*<")

HUNK_PATTERN = re.compile(r"^@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@")


def run_git(args):
    """Runs git in the repository root and returns (exit code, stdout bytes)."""
    proc = subprocess.run(["git", "-c", "core.quotepath=off"] + args, cwd=ROOT,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return proc.returncode, proc.stdout


def decode(data):
    """Decodes bytes without losing control characters."""
    return data.decode("utf-8", errors="replace")


def split_lines(text):
    """Splits on \\n only; str.splitlines would also split on 0x0B, 0x0C and 0x1C-0x1E."""
    return text.split("\n")


def load_hook_module():
    """Loads the pre-push hook script as a module to share its term list reading."""
    path = os.path.join(ROOT, HOOK_SCRIPT)
    spec = importlib.util.spec_from_file_location("check_protected_content", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def merge_base(base):
    """Returns the merge base of base and HEAD, or None if the base does not exist."""
    code, out = run_git(["merge-base", base, "HEAD"])
    if code != 0:
        return None
    return decode(out).strip()


def parse_diff(diff_text):
    """Returns {path: [(line number, text), ...]} of the added lines of a -U0 diff."""
    added = {}
    path = None
    line_no = 0
    for line in split_lines(diff_text):
        if line.startswith("+++ "):
            target = line[4:].rstrip("\r")
            path = target[2:] if target.startswith("b/") else None
            if path is not None:
                added.setdefault(path, [])
            continue
        if line.startswith("--- ") or line.startswith("diff ") or line.startswith("index "):
            continue
        match = HUNK_PATTERN.match(line)
        if match:
            line_no = int(match.group(1))
            continue
        if path is not None and line.startswith("+"):
            added[path].append((line_no, line[1:]))
            line_no += 1
    return added


def read_untracked():
    """Returns (added lines by path, list of untracked paths); every line counts as added."""
    code, out = run_git(["ls-files", "--others", "--exclude-standard"])
    paths = [p for p in decode(out).split("\n") if p != ""] if code == 0 else []
    added = {}
    for path in paths:
        try:
            with open(os.path.join(ROOT, path), "rb") as handle:
                data = handle.read()
        except OSError:
            continue
        if b"\x00" in data[:8192] and not path.endswith(".txt"):
            # Binary file: no text lines to scan (a control char scan of an image is noise).
            added[path] = []
            continue
        added[path] = list(enumerate(split_lines(decode(data)), start=1))
    return added, paths


def collect_changes(base_commit):
    """Returns (added lines by path, added or modified paths) against the base commit."""
    code, out = run_git(["diff", "-U0", "--no-color", "--no-renames", base_commit])
    added = parse_diff(decode(out)) if code == 0 else {}
    code, out = run_git(["diff", "--name-only", "--diff-filter=AM", "--no-renames",
                         base_commit])
    changed = [p for p in decode(out).split("\n") if p != ""] if code == 0 else []
    untracked_lines, untracked_paths = read_untracked()
    for path, lines in untracked_lines.items():
        added[path] = lines
    return added, changed + untracked_paths


def strip_line_comment(text):
    """Removes a trailing // comment (good enough for the rules; strings are not parsed)."""
    index = text.find("//")
    return text if index < 0 else text[:index]


def is_cpp_file(path):
    """True for C++ sources and headers."""
    return path.endswith(CPP_EXTENSIONS)


def is_probe_script(path):
    """True for check scripts, whose C++ probe text gets the C++ rules."""
    return path.endswith(".py") and path.startswith(PROBE_SCRIPT_DIRS) and path != SELF_PATH


def check_cpp_rules(path, lines, violations):
    """Applies the auto and lambda rules to the added lines of one file."""
    previous_text = ""
    for line_no, text in lines:
        code = strip_line_comment(text)
        stripped = code.strip()
        is_comment_only = text.strip().startswith("//") or text.strip().startswith("*")
        if not is_comment_only and not stripped.startswith("#"):
            if AUTO_PATTERN.search(code):
                violations.append((path, line_no, "auto", "write the type out instead of auto"))
            has_lambda = STD_FUNCTION_PATTERN.search(code) is not None
            for pattern in LAMBDA_PATTERNS:
                if pattern.search(code):
                    has_lambda = True
            excused = LAMBDA_MARKER in text or LAMBDA_MARKER in previous_text
            if has_lambda and not excused:
                violations.append((path, line_no, "lambda",
                                   "use a named function; no lambda or std::function "
                                   "(marker: // %s <reason>)" % LAMBDA_MARKER))
        previous_text = text


def check_control_chars(path, lines, violations):
    """Reports added lines that contain control characters."""
    for line_no, text in lines:
        match = CONTROL_CHAR_PATTERN.search(text)
        if match:
            violations.append((path, line_no, "control-char",
                               "control character 0x%02X" % ord(match.group(0))))


def credits_text():
    """Reads the CREDITS file of the working tree."""
    for name in CREDITS_FILE_NAMES:
        path = os.path.join(ROOT, name)
        if os.path.isfile(path):
            with open(path, "r", encoding="utf-8", errors="replace") as handle:
                return handle.read()
    return ""


def is_asset_covered(path, credits, hook):
    """True if CREDITS covers the asset: the hook's rules, or a bare file name entry."""
    if hook.is_covered_by_credits(path, credits):
        return True
    base_name = os.path.basename(path)
    tokens = re.split(r"[\s`,;()\[\]<>\"']+", credits)
    return base_name in tokens


def check_credits(changed_paths, hook, violations):
    """Reports new or changed assets without a CREDITS entry."""
    credits = credits_text()
    for path in changed_paths:
        if path.lower().endswith(ASSET_EXTENSIONS) and not is_asset_covered(path, credits,
                                                                            hook):
            violations.append((path, 0, "credits", "asset has no entry in CREDITS"))


def check_protected_terms(added, hook, violations):
    """Reports added paths and lines that contain a word of the hook's term list."""
    terms, error = hook.find_terms(ROOT, None)
    if terms is None:
        print("SKIPPED protected-term rule: %s" % error)
        return
    for path, lines in added.items():
        if hook.find_term(path, terms) is not None:
            violations.append((path, 0, "protected-term", "path contains a blocked word"))
        for prefix in hook.BLOCKED_PREFIXES:
            if path.startswith(prefix):
                violations.append((path, 0, "protected-term", "path is in a blocked folder"))
        for line_no, text in lines:
            if hook.find_term(text, terms) is not None:
                violations.append((path, line_no, "protected-term",
                                   "line contains a blocked word"))


def main(argv):
    """Entry point: prints every violation and returns the exit code."""
    parser = argparse.ArgumentParser(description="Coding rules check for added lines.")
    parser.add_argument("--base", default=DEFAULT_BASE,
                        help="base ref of the diff (default %s)" % DEFAULT_BASE)
    args = parser.parse_args(argv[1:])
    base_commit = merge_base(args.base)
    if base_commit is None:
        print("base %s not found (git fetch origin first)" % args.base)
        return 2
    hook = load_hook_module()
    added, changed_paths = collect_changes(base_commit)
    violations = []
    for path, lines in sorted(added.items()):
        if path == SELF_PATH:
            continue
        if is_cpp_file(path) or is_probe_script(path):
            check_cpp_rules(path, lines, violations)
        check_control_chars(path, lines, violations)
    check_credits([p for p in changed_paths if p != SELF_PATH], hook, violations)
    check_protected_terms({p: l for p, l in added.items() if p != SELF_PATH}, hook,
                          violations)
    for path, line_no, rule, message in violations:
        print("%s:%d: %s: %s" % (path, line_no, rule, message))
    if violations:
        print("%d violation(s)" % len(violations))
        return 1
    print("coding rules: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
