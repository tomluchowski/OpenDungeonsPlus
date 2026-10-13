#!/usr/bin/env python3
"""Pre-push check for protected content.

Installed as the git pre-push hook. Git passes
"<local ref> <local sha> <remote ref> <remote sha>" lines on stdin and the remote
name and url as arguments. The push is blocked (exit 1) if

  * a term from the local term list is in a commit message or a ref name (all commits
    of the pushed range, new or rewritten),
  * the file tree at the pushed tip contains a term in a file name or in a line of a
    text file (lines that also exist in the baseline <remote>/main are ignored, binary
    files are checked by name only), has files under levels/campaign/ that are not in
    CAMPAIGN_ALLOW_LIST, or has anything under tools/level-convert/,
  * a commit with new content (its patch is not part of the history already on the
    remote) has a term in a file name or an added line, adds or changes a file under
    a blocked path (files already on the branch before are not checked), or adds media files that are
    not covered by the CREDITS file at the pushed tip. Commits that were only rewritten
    from history already on the remote are not diffed again.

The term list is a local file outside version control. Its repo-relative path is read
from the git config key protectedcontent.terms. Besides terms (one per line, "#" comments)
it may contain lines "path: <prefix>" that name blocked path prefixes and lines
"word: <term>" whose term only matches as a whole word (case-insensitive; no letter, digit
or underscore directly before or after it). Lines without a prefix match as substrings, as
before. An older copy of this script that does not know "word:" reads the whole line as a
substring that practically never occurs, so it blocks nothing extra. If the term list is
not configured, missing or empty the push is blocked as well (fail safe).

Without ref lines on stdin (a terminal, an empty stdin or no data within 5 seconds) the
script ends with exit code 1 instead of waiting.

Usage:
  check-protected-content.py [--terms <file>] [<remote> [<url>]] < ref-lines
  check-protected-content.py --self-test
"""

import fnmatch
import os
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time

ZERO_SHA = "0" * 40

# Seconds to wait for the ref lines on stdin before the check gives up.
STDIN_TIMEOUT_SECONDS = 5
NO_REF_LINES_MESSAGE = ("keine Ref-Zeilen auf stdin, als Pre-Push-Hook mit Ref-Zeilen "
                        "aufrufen")

TERMS_CONFIG_KEY = "protectedcontent.terms"
PATH_DIRECTIVE = "path:"
WORD_DIRECTIVE = "word:"

# Blocked path prefixes, filled from the "path:" lines of the term list.
BLOCKED_PREFIXES = []

# Placeholder campaign files that exist in origin/integration/all.
CAMPAIGN_ALLOW_LIST = [
    "levels/campaign/Bonus1.level",
    "levels/campaign/Campaign.cfg",
    "levels/campaign/Campaign1.level",
    "levels/campaign/Campaign2.level",
    "levels/campaign/Campaign3.level",
    "levels/campaign/Mossgate.level",
    "levels/campaign/Brackenford.level",
    "levels/campaign/Coldwell.level",
    "levels/campaign/Tinmoor.level",
    "levels/campaign/Ravensledge.level",
    "levels/campaign/Ashcombe.level",
    "levels/campaign/Greywater.level",
    "levels/campaign/HollinFen.level",
    "levels/campaign/Saltmere.level",
    "levels/campaign/Dunmarrow.level",
    "levels/campaign/Ironbridge.level",
    "levels/campaign/Wolfscar.level",
    "levels/campaign/Lanternhill.level",
    "levels/campaign/Cinderhollow.level",
    "levels/campaign/Thornreach.level",
    "levels/campaign/Bellwick.level",
    "levels/campaign/Highcairn.level",
    "levels/campaign/Stormhaven.level",
    "levels/campaign/Mirewatch.level",
    "levels/campaign/Goldspire.level",
    "levels/campaign/Ebonrook.level",
    "levels/campaign/VarnsCrossing.level",
    "levels/campaign/Silverdeep.level",
    "levels/campaign/Wraithwood.level",
    "levels/campaign/Kingsfall.level",
    "levels/campaign/HollowmarkCitadel.level",
    "levels/campaign/BoulderCourse.level",
    "levels/campaign/CrowshotGallery.level",
    "levels/campaign/TwistingHalls.level",
    "levels/campaign/SkittleCavern.level",
    "levels/campaign/SwarmNight.level",
]

CAMPAIGN_PREFIX = "levels/campaign/"
CONVERTER_PREFIX = "tools/level-convert/"

MEDIA_EXTENSIONS = set([
    "png", "jpg", "jpeg", "gif", "bmp", "tga", "dds", "tif", "tiff", "svg",
    "ogg", "wav", "mp3", "flac", "mid", "mesh", "skeleton", "glb", "gltf",
    "fbx", "obj", "blend", "ico", "ttf", "otf", "mp4", "avi", "ogv",
])

CREDITS_NAMES = ["CREDITS", "CREDITS.md", "CREDITS.txt"]

LEVEL_PREFIX = "levels/"
LEVEL_SUFFIXES = (".level", ".cfg")
SIMILARITY_SCRIPT = "check-level-similarity.py"
PROGRESSION_SCRIPT = "check-campaign-progression.py"


def run_git(args, cwd):
    proc = subprocess.run(["git"] + args, cwd=cwd, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE)
    return proc.returncode, proc.stdout.decode("utf-8", errors="replace")


class WordTerm(str):
    """A term that only matches as a whole word."""

    def __new__(cls, text):
        instance = str.__new__(cls, text)
        instance.pattern = re.compile(r"(?<!\w)" + re.escape(text) + r"(?!\w)")
        return instance


def read_terms_file(path):
    terms = []
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            line = line.strip()
            if line == "" or line.startswith("#"):
                continue
            if line.lower().startswith(PATH_DIRECTIVE):
                prefix = line[len(PATH_DIRECTIVE):].strip()
                if prefix != "" and prefix not in BLOCKED_PREFIXES:
                    BLOCKED_PREFIXES.append(prefix)
                continue
            if line.lower().startswith(WORD_DIRECTIVE):
                word = line[len(WORD_DIRECTIVE):].strip().lower()
                if word != "":
                    terms.append(WordTerm(word))
                continue
            terms.append(line.lower())
    return terms


def find_terms_file(cwd):
    code, relative = run_git(["config", "--get", TERMS_CONFIG_KEY], cwd)
    relative = relative.strip()
    if code != 0 or relative == "":
        return None
    candidates = []
    code, out = run_git(["rev-parse", "--git-common-dir"], cwd)
    if code == 0:
        common = os.path.abspath(os.path.join(cwd, out.strip()))
        candidates.append(os.path.join(os.path.dirname(common), relative))
    code, out = run_git(["rev-parse", "--show-toplevel"], cwd)
    if code == 0:
        candidates.append(os.path.join(out.strip(), relative))
    for candidate in candidates:
        if os.path.isfile(candidate):
            return candidate
    return None


def find_terms(cwd, explicit_path):
    """Returns (terms, error message)."""
    path = explicit_path
    if path is None:
        path = find_terms_file(cwd)
    if path is None or not os.path.isfile(path):
        return None, ("The protected term list is missing (set the git config key %s to "
                      "its path relative to the main working tree). Push blocked."
                      % TERMS_CONFIG_KEY)
    terms = read_terms_file(path)
    if len(terms) == 0:
        return None, "The protected term list %s is empty. Push blocked." % path
    return terms, None


def find_term(text, terms):
    lowered = text.lower()
    for term in terms:
        if isinstance(term, WordTerm):
            if term.pattern.search(lowered):
                return term
        elif term in lowered:
            return term
    return None


def split_lines(text):
    return [line for line in text.split("\n") if line != ""]


def credits_text(tip, cwd):
    for name in CREDITS_NAMES:
        code, out = run_git(["show", "%s:%s" % (tip, name)], cwd)
        if code == 0:
            return out
    return ""


def is_covered_by_credits(path, credits):
    tokens = re.split(r"[\s`,;()\[\]<>\"']+", credits)
    for token in tokens:
        token = token.strip()
        if token == "":
            continue
        if token == path:
            return True
        if "/" in token and not any(c in token for c in "*?["):
            if path.startswith(token.rstrip("/") + "/"):
                return True
        if any(c in token for c in "*?["):
            if fnmatch.fnmatch(path, token):
                return True
            if "/" not in token and fnmatch.fnmatch(os.path.basename(path), token):
                return True
    return False


def find_baseline(remote_name, cwd):
    """The mirror of the project the fork was made from: <remote>/main, or None."""
    ref = "refs/remotes/%s/main" % remote_name
    code, out = run_git(["rev-parse", "--verify", "-q", ref + "^{commit}"], cwd)
    if code != 0:
        return None
    return out.strip()


def commits_to_check(local_sha, remote_sha, remote_name, cwd):
    if remote_sha != ZERO_SHA:
        code, out = run_git(["rev-list", "%s..%s" % (remote_sha, local_sha)], cwd)
        if code == 0:
            return split_lines(out)
    code, out = run_git(["rev-list", local_sha, "--not", "--remotes"], cwd)
    if code != 0:
        return []
    return split_lines(out)


def patch_ids(revisions, cwd):
    """Maps commit -> patch id for the non-merge commits of the given rev-list args. The
    revisions go through stdin: a rebase can list thousands of them, more than a Windows
    command line holds (WinError 206)."""
    data = ("\n".join(revisions) + "\n").encode("utf-8")
    proc = subprocess.run(["git", "log", "-p", "--no-color", "--no-merges", "--stdin"],
                          cwd=cwd, input=data, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if proc.returncode != 0:
        return {}
    ids = subprocess.run(["git", "patch-id", "--stable"], cwd=cwd, input=proc.stdout,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    result = {}
    for line in ids.stdout.decode("utf-8", errors="replace").split("\n"):
        fields = line.split()
        if len(fields) == 2:
            result[fields[1]] = fields[0]
    return result


def known_patch_ids(remote_sha, baseline, cwd):
    """Patch ids of the history that is already on the remote (without the baseline)."""
    if remote_sha == ZERO_SHA:
        return set()
    revisions = [remote_sha]
    if baseline is not None:
        revisions += ["--not", baseline]
    return set(patch_ids(revisions, cwd).values())


def check_commit_message(commit, terms, cwd, problems):
    code, message = run_git(["log", "-1", "--format=%B", commit], cwd)
    term = find_term(message, terms)
    if term is not None:
        problems.append("commit %s: term '%s' in the commit message" % (commit[:10], term))


def check_commit_content(commit, terms, cwd, added_files, problems):
    short = commit[:10]
    code, parents = run_git(["rev-list", "--parents", "-n", "1", commit], cwd)
    if len(parents.split()) > 2:
        return  # merge commit: the merged commits are checked on their own

    code, names = run_git(["diff-tree", "--root", "--no-commit-id", "-r", "-M",
                           "--name-status", commit], cwd)
    for line in split_lines(names):
        fields = line.split("\t")
        status = fields[0][0]
        path = fields[-1]
        if status == "D":
            continue
        term = find_term(path, terms)
        if term is not None:
            problems.append("commit %s: term '%s' in file name %s" % (short, term, path))
        for prefix in BLOCKED_PREFIXES:
            if path.startswith(prefix):
                problems.append("commit %s: %s is under the blocked path %s (never "
                                "pushed)" % (short, path, prefix))
        extension = path.rsplit(".", 1)[-1].lower() if "." in os.path.basename(path) else ""
        if status in ("A", "C", "R") and extension in MEDIA_EXTENSIONS:
            added_files.setdefault(path, short)

    code, diff = run_git(["show", "--format=", "-p", "--no-color", "-U0", commit], cwd)
    for line in diff.split("\n"):
        if line.startswith("+") and not line.startswith("+++"):
            term = find_term(line, terms)
            if term is not None:
                problems.append("commit %s: term '%s' in an added line: %s"
                                % (short, term, line[:80]))


def baseline_lines(baseline, path, cache, cwd):
    if baseline is None:
        return set()
    if path not in cache:
        code, out = run_git(["show", "%s:%s" % (baseline, path)], cwd)
        cache[path] = set(out.split("\n")) if code == 0 else set()
    return cache[path]


def check_tree(tip, baseline, terms, cwd, problems):
    code, names = run_git(["ls-tree", "-r", "--name-only", tip], cwd)
    for path in split_lines(names):
        term = find_term(path, terms)
        if term is not None:
            problems.append("tree: term '%s' in file name %s" % (term, path))
        if path.startswith(CAMPAIGN_PREFIX) and path not in CAMPAIGN_ALLOW_LIST:
            problems.append("tree: %s is not in the campaign allow list" % path)
        if path.startswith(CONVERTER_PREFIX):
            problems.append("tree: %s is under %s" % (path, CONVERTER_PREFIX))

    # Pre-filter with git grep; find_term below makes the final decision. Whole-word terms
    # get a word-boundary pattern so that a short word does not select every file.
    hits = ""
    plain = [term for term in terms if not isinstance(term, WordTerm)]
    words = [term for term in terms if isinstance(term, WordTerm)]
    if len(plain) > 0:
        arguments = ["grep", "-I", "-i", "-n", "-F", "--no-color"]
        for term in plain:
            arguments += ["-e", term]
        code, found = run_git(arguments + [tip], cwd)
        hits += found
    if len(words) > 0:
        arguments = ["grep", "-I", "-i", "-n", "-E", "--no-color"]
        for term in words:
            escaped = re.sub(r"([.^$*+?()\[\]{}|\\])", r"\\\1", str(term))
            arguments += ["-e", "(^|[^[:alnum:]_])" + escaped + "([^[:alnum:]_]|$)"]
        code, found = run_git(arguments + [tip], cwd)
        hits += found
    cache = {}
    prefix = tip + ":"
    for hit in split_lines(hits):
        if hit.startswith(prefix):
            hit = hit[len(prefix):]
        fields = hit.split(":", 2)
        if len(fields) != 3:
            continue
        path, number, text = fields
        if text in baseline_lines(baseline, path, cache, cwd):
            continue
        term = find_term(text, terms)
        if term is not None:
            problems.append("tree: term '%s' in %s:%s: %s" % (term, path, number, text[:80]))


def check_push(ref_lines, terms, remote_name, cwd):
    problems = []
    baseline = find_baseline(remote_name, cwd)
    for ref_line in ref_lines:
        fields = ref_line.split()
        if len(fields) != 4:
            continue
        local_ref, local_sha, remote_ref, remote_sha = fields
        for ref in (local_ref, remote_ref):
            term = find_term(ref, terms)
            if term is not None:
                problems.append("ref %s: term '%s' in the ref name" % (ref, term))
        if local_sha == ZERO_SHA:
            continue  # deleted ref: only the name is checked
        code, tip = run_git(["rev-parse", "%s^{commit}" % local_sha], cwd)
        if code != 0:
            continue  # tag pointing to a non-commit
        tip = tip.strip()
        commits = commits_to_check(tip, remote_sha, remote_name, cwd)
        known = known_patch_ids(remote_sha, baseline, cwd)
        own_ids = {}
        if len(commits) > 0:
            own_ids = patch_ids(["--no-walk"] + commits, cwd)
        added_files = {}
        for commit in commits:
            check_commit_message(commit, terms, cwd, problems)
            if commit in own_ids and own_ids[commit] in known:
                continue  # rewritten history that is already on the remote
            check_commit_content(commit, terms, cwd, added_files, problems)
        check_tree(tip, baseline, terms, cwd, problems)
        if added_files:
            credits = credits_text(tip, cwd)
            for path in sorted(added_files):
                code, listed = run_git(["ls-tree", "--name-only", tip, "--", path], cwd)
                if listed.strip() == "":
                    continue  # no longer present at the tip
                if not is_covered_by_credits(path, credits):
                    problems.append("commit %s: new media file %s is not mentioned in "
                                    "CREDITS" % (added_files[path], path))
    return problems


def changed_level_files(tip, commits, cwd):
    """Level files under levels/ that the given commits add or change and that exist at tip."""
    paths = set()
    for commit in commits:
        code, parents = run_git(["rev-list", "--parents", "-n", "1", commit], cwd)
        if len(parents.split()) > 2:
            continue
        code, names = run_git(["diff-tree", "--root", "--no-commit-id", "-r", "-M",
                               "--name-status", commit], cwd)
        for line in split_lines(names):
            fields = line.split("\t")
            path = fields[-1]
            if (fields[0][0] != "D" and path.startswith(LEVEL_PREFIX)
                    and path.endswith(LEVEL_SUFFIXES)):
                paths.add(path)
    return sorted(path for path in paths
                  if run_git(["cat-file", "-e", "%s:%s" % (tip, path)], cwd)[0] == 0)


def check_level_similarity(ref_lines, remote_name, cwd, problems):
    """Runs the level similarity check on new or changed levels; it skips itself when the
    local folder with the other levels does not exist."""
    script = os.path.join(os.path.dirname(os.path.abspath(__file__)), SIMILARITY_SCRIPT)
    if not os.path.isfile(script):
        return
    for ref_line in ref_lines:
        fields = ref_line.split()
        if len(fields) != 4 or fields[1] == ZERO_SHA:
            continue
        code, tip = run_git(["rev-parse", "%s^{commit}" % fields[1]], cwd)
        if code != 0:
            continue
        tip = tip.strip()
        paths = changed_level_files(tip, commits_to_check(tip, fields[3], remote_name, cwd),
                                    cwd)
        if not paths:
            continue
        work = tempfile.mkdtemp(prefix="level-similarity-")
        try:
            for path in paths:
                proc = subprocess.run(["git", "show", "%s:%s" % (tip, path)], cwd=cwd,
                                      stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                target = os.path.join(work, path)
                os.makedirs(os.path.dirname(target), exist_ok=True)
                with open(target, "wb") as handle:
                    handle.write(proc.stdout)
            proc = subprocess.run([sys.executable, script] + paths, cwd=work,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            for line in proc.stdout.decode("utf-8", errors="replace").split("\n"):
                if line.startswith("FAIL"):
                    problems.append("similarity: %s" % line.strip())
        finally:
            shutil.rmtree(work, ignore_errors=True)


def check_campaign_progression(ref_lines, remote_name, cwd, problems):
    """Runs the unlock check on new or changed campaign levels; it skips itself when the
    local folder with the other levels does not exist."""
    script = os.path.join(os.path.dirname(os.path.abspath(__file__)), PROGRESSION_SCRIPT)
    if not os.path.isfile(script):
        return
    for ref_line in ref_lines:
        fields = ref_line.split()
        if len(fields) != 4 or fields[1] == ZERO_SHA:
            continue
        code, tip = run_git(["rev-parse", "%s^{commit}" % fields[1]], cwd)
        if code != 0:
            continue
        tip = tip.strip()
        paths = [path for path in changed_level_files(
                     tip, commits_to_check(tip, fields[3], remote_name, cwd), cwd)
                 if (path.startswith(CAMPAIGN_PREFIX) or path.startswith("levels/skirmish/"))
                 and path.endswith(".level")]
        if not paths:
            continue
        work = tempfile.mkdtemp(prefix="campaign-progression-")
        try:
            for path in paths:
                proc = subprocess.run(["git", "show", "%s:%s" % (tip, path)], cwd=cwd,
                                      stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                target = os.path.join(work, path)
                os.makedirs(os.path.dirname(target), exist_ok=True)
                with open(target, "wb") as handle:
                    handle.write(proc.stdout)
            proc = subprocess.run([sys.executable, script] + paths, cwd=work,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            for line in proc.stdout.decode("utf-8", errors="replace").split("\n"):
                if line.startswith("FAIL"):
                    problems.append("progression: %s" % line.strip())
        finally:
            shutil.rmtree(work, ignore_errors=True)


def report_and_exit(problems):
    sys.stderr.write("Push blocked by check-protected-content:\n")
    for problem in problems:
        sys.stderr.write("  - %s\n" % problem)
    sys.stderr.write("Fix the commits (do not bypass this check without an owner "
                     "decision).\n")
    return 1


def self_test():
    failures = []
    works = []

    def new_repo():
        work = tempfile.mkdtemp(prefix="protected-selftest-")
        works.append(work)

        def git(*args):
            proc = subprocess.run(["git"] + list(args), cwd=work, stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE)
            if proc.returncode != 0:
                raise RuntimeError("git %s failed: %s" % (args, proc.stderr.decode()))
            return proc.stdout.decode().strip()

        def commit_files(files, message):
            for name, content in files.items():
                full = os.path.join(work, name)
                os.makedirs(os.path.dirname(full) or work, exist_ok=True)
                mode = "wb" if isinstance(content, bytes) else "w"
                with open(full, mode) as handle:
                    handle.write(content)
                git("add", name)
            git("commit", "-q", "-m", message)
            return git("rev-parse", "HEAD")

        git("init", "-q")
        git("config", "user.email", "test@example.invalid")
        git("config", "user.name", "Test")
        base = commit_files({"readme.txt": "hello\n"}, "base")
        git("update-ref", "refs/remotes/origin/main", base)
        return work, git, commit_files, base

    def expect(name, work, lines, terms, should_block, needle=None, absent=None):
        problems = check_push(lines, terms, "origin", work)
        blocked = len(problems) > 0
        if blocked != should_block or (needle and not any(needle in p for p in problems)):
            failures.append("%s: blocked=%s problems=%s" % (name, blocked, problems))
        if absent and any(absent in p for p in problems):
            failures.append("%s: unexpected '%s' in %s" % (name, absent, problems))

    def one_commit_scenario(name, files, message, terms, should_block, needle=None,
                            ref="refs/heads/topic"):
        work, git, commit_files, base = new_repo()
        sha = commit_files(files, message)
        expect(name, work, ["%s %s %s %s" % (ref, sha, ref, base)], terms, should_block,
               needle)

    try:
        terms = ["zzterm"]
        word_terms = [WordTerm("zork"), "zzterm"]
        for text, expected in (("zork", True), ("Zork!", True), ("a zork b", True),
                               ("a-zork-b", True), ("zorkmid", False), ("unzork", False),
                               ("zork_x", False), ("zork2", False), ("", False)):
            if (find_term(text, word_terms) is not None) != expected:
                failures.append("word term on %r must %smatch" % (text,
                                "" if expected else "not "))
        if find_term("xxzzterm", word_terms) is None:
            failures.append("substring term must match inside words")
        word_file = os.path.join(tempfile.gettempdir(), "odp-word-terms-selftest.txt")
        with open(word_file, "w") as handle:
            handle.write("word: Zork\nplain\n")
        read_back = read_terms_file(word_file)
        os.remove(word_file)
        if len(read_back) != 2 or not isinstance(read_back[0], WordTerm) or                 isinstance(read_back[1], WordTerm):
            failures.append("term list parsing of word: lines failed")
        del BLOCKED_PREFIXES[:]
        BLOCKED_PREFIXES.append("blocked-area/")
        one_commit_scenario("clean", {"a.txt": "fine\n"}, "clean change", terms, False)
        one_commit_scenario("message", {"b.txt": "x\n"}, "mentions ZZTerm here", terms,
                            True, "commit message")
        one_commit_scenario("added line", {"c.txt": "line with zzterm inside\n"}, "add c",
                            terms, True, "added line")
        word_scenario = [WordTerm("zork")]
        one_commit_scenario("word inside word", {"w.txt": "zorkmid important\n"},
                            "unzork", word_scenario, False)
        one_commit_scenario("whole word line", {"w2.txt": "a zork b\n"}, "add w2",
                            word_scenario, True, "added line")
        one_commit_scenario("whole word message", {"w3.txt": "x\n"}, "Zork!",
                            word_scenario, True, "commit message")
        one_commit_scenario("file name", {"zzterm-file.txt": "x\n"}, "add file", terms,
                            True, "file name")
        one_commit_scenario("binary content ignored", {"bin.dat": b"\x00zzterm\x00"},
                            "binary content", terms, False)
        one_commit_scenario("ref name", {"a.txt": "fine\n"}, "clean change", terms, True,
                            "ref name", "refs/heads/feature/zzterm-x")
        one_commit_scenario("allowed level", {"levels/campaign/Campaign1.level": "ok\n"},
                            "allowed level", terms, False)
        one_commit_scenario("campaign", {"levels/campaign/Other.level": "x\n"},
                            "other level", terms, True, "allow list")
        one_commit_scenario("converter", {"tools/level-convert/run.py": "x\n"},
                            "converter", terms, True, "tools/level-convert/")
        one_commit_scenario("blocked path added", {"blocked-area/PLAN.md": "x\n"},
                            "blocked path file", terms, True, "blocked-area/")
        work, git, commit_files, base = new_repo()
        earlier = commit_files({"blocked-area/PLAN.md": "x\n"}, "blocked path file")
        changed = commit_files({"blocked-area/PLAN.md": "y\n"}, "blocked path changed")
        expect("blocked path changed", work,
               ["refs/heads/topic %s refs/heads/topic %s" % (changed, earlier)], terms,
               True, "blocked-area/")
        unrelated = commit_files({"d.txt": "fine\n"}, "unrelated change")
        expect("blocked path already on branch", work,
               ["refs/heads/topic %s refs/heads/topic %s" % (unrelated, changed)], terms,
               False)
        one_commit_scenario("media without credits", {"gfx/pic.png": b"\x89PNG"},
                            "add picture", terms, True, "CREDITS")
        one_commit_scenario("media with glob",
                            {"gfx/pic.png": b"\x89PNG", "CREDITS": "gfx/*.png - own, CC0\n"},
                            "add picture", terms, False)
        one_commit_scenario("media with directory",
                            {"snd/a.ogg": b"OggS", "CREDITS": "snd/ - own, CC0\n"},
                            "add sound", terms, False)

        work, git, commit_files, base = new_repo()
        git("branch", "-q", "newbranch")
        sha = commit_files({"c.txt": "zzterm\n"}, "add c")
        expect("new branch", work, ["refs/heads/newbranch %s refs/heads/newbranch %s"
                                    % (sha, ZERO_SHA)], terms, True)

        work, git, commit_files, base = new_repo()
        expect("deleted ref", work, ["(delete) %s refs/heads/old %s" % (ZERO_SHA, base)],
               terms, False)
        expect("deleted ref name", work, ["(delete) %s refs/heads/zzterm %s"
                                          % (ZERO_SHA, base)], terms, True, "ref name")

        # the tree at the tip is checked, lines that exist in the baseline are not
        work, git, commit_files, base = new_repo()
        mirror = commit_files({"up.txt": "an upstream zzterm line\n"}, "upstream")
        git("update-ref", "refs/remotes/origin/main", mirror)
        sha = commit_files({"own.txt": "fine\n"}, "own change")
        expect("baseline line ignored", work, ["refs/heads/t %s refs/heads/t %s"
                                               % (sha, mirror)], terms, False)
        sha = commit_files({"up.txt": "an upstream zzterm line\nnew zzterm line\n"},
                           "edit upstream file")
        expect("new line in upstream file", work, ["refs/heads/t %s refs/heads/t %s"
                                                   % (sha, mirror)], terms, True, "tree")

        # a rewritten commit is not diffed again, but the tree still counts
        work, git, commit_files, base = new_repo()
        old = commit_files({"c.txt": "line with zzterm inside\n"}, "add c")
        git("commit", "-q", "--amend", "-m", "add c, neutral wording")
        rewritten = git("rev-parse", "HEAD")
        expect("rewritten commit", work, ["refs/heads/t %s refs/heads/t %s"
                                          % (rewritten, old)], terms, True, "tree",
               "added line")
        git("rm", "-q", "c.txt")
        git("commit", "-q", "-m", "remove c")
        cleaned = git("rev-parse", "HEAD")
        expect("rewritten commit, tip clean", work, ["refs/heads/t %s refs/heads/t %s"
                                                     % (cleaned, old)], terms, False,
               None, "added line")
        # the message of a rewritten commit is still checked
        git("commit", "-q", "--allow-empty", "-m", "zzterm in a message")
        dirty = git("rev-parse", "HEAD")
        expect("rewritten message", work, ["refs/heads/t %s refs/heads/t %s"
                                           % (dirty, old)], terms, True, "commit message")

        # a rebase lists the whole new history: far more revisions than one command line
        # holds on Windows (WinError 206 before the revisions went through stdin)
        work, git, commit_files, base = new_repo()
        second = commit_files({"second.txt": "second\n"}, "second base")

        def fast_import_chain(ref, parent, count):
            stream = []
            for number in range(count):
                text = "line %d" % number
                message = "change %d" % number
                stream += ["commit %s" % ref,
                           "committer T <t@example.invalid> %d +0000" % (number + 1),
                           "data %d" % len(message), message]
                if number == 0:
                    stream += ["from %s" % parent]
                stream += ["M 100644 inline f%d.txt" % number, "data %d" % len(text), text, ""]
            proc = subprocess.run(["git", "fast-import", "--quiet"], cwd=work,
                                  input=("\n".join(stream) + "\n").encode("utf-8"),
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if proc.returncode != 0:
                raise RuntimeError("fast-import failed: %s" % proc.stderr.decode())
            return git("rev-parse", ref)

        # the same changes on two bases: the pushed tip is the rebased copy of the old one
        old = fast_import_chain("refs/heads/old", base, 900)
        rebased = fast_import_chain("refs/heads/rebased", second, 900)
        expect("rebase with many commits", work, ["refs/heads/rebased %s refs/heads/rebased %s"
                                                  % (rebased, old)], terms, False)

        # run as a pre-push hook: stdin without ref lines must fail fast, with ref lines
        # the normal check runs
        work, git, commit_files, base = new_repo()
        hook_terms = os.path.join(work, "hook-terms.txt")
        with open(hook_terms, "w") as handle:
            handle.write("zzterm\n")
        hook_command = [sys.executable, os.path.abspath(__file__), "--terms", hook_terms,
                        "origin"]

        def run_hook(stdin_mode):
            """stdin_mode is "devnull", "open" (a pipe that stays open without data) or
            the text to send on stdin. Returns (exit code, stderr, seconds) or None."""
            stdin_target = subprocess.DEVNULL if stdin_mode == "devnull" else subprocess.PIPE
            started = time.time()
            proc = subprocess.Popen(hook_command, cwd=work, stdin=stdin_target,
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if stdin_mode not in ("devnull", "open"):
                proc.stdin.write(stdin_mode.encode("utf-8"))
                proc.stdin.close()
            try:
                proc.wait(timeout=30)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
                failures.append("hook with stdin '%s' did not end" % stdin_mode[:20])
                return None
            finally:
                if proc.stdin is not None and not proc.stdin.closed:
                    proc.stdin.close()
            seconds = time.time() - started
            error = proc.stderr.read().decode("utf-8", errors="replace")
            proc.stdout.close()
            proc.stderr.close()
            return proc.returncode, error, seconds

        for mode, limit in (("devnull", STDIN_TIMEOUT_SECONDS),
                            ("open", STDIN_TIMEOUT_SECONDS + 10)):
            result = run_hook(mode)
            if result is None:
                continue
            code, error, seconds = result
            if code == 0 or NO_REF_LINES_MESSAGE not in error or seconds >= limit:
                failures.append("hook without ref lines (%s): exit=%s seconds=%.1f stderr=%r"
                                % (mode, code, seconds, error))
        clean = commit_files({"hook-a.txt": "fine\n"}, "clean change")
        result = run_hook("refs/heads/topic %s refs/heads/topic %s\n" % (clean, base))
        if result is not None and result[0] != 0:
            failures.append("hook with a clean ref line: exit=%s stderr=%r"
                            % (result[0], result[1]))
        dirty = commit_files({"hook-b.txt": "line with zzterm inside\n"}, "add b")
        result = run_hook("refs/heads/topic %s refs/heads/topic %s\n" % (dirty, base))
        if result is not None and (result[0] != 1 or "Push blocked" not in result[1]):
            failures.append("hook with a dirty ref line: exit=%s stderr=%r"
                            % (result[0], result[1]))

        # fail safe on the term list
        empty = os.path.join(work, "empty-terms.txt")
        with open(empty, "w") as handle:
            handle.write("# nothing\n")
        found, error = find_terms(work, empty)
        if found is not None or error is None:
            failures.append("empty term list must block")
        found, error = find_terms(work, os.path.join(work, "does-not-exist.txt"))
        if found is not None or error is None:
            failures.append("missing term list must block")
    finally:
        for work in works:
            shutil.rmtree(work, ignore_errors=True)

    if failures:
        for failure in failures:
            sys.stderr.write("SELF-TEST FAIL: %s\n" % failure)
        return 1
    print("self-test passed")
    return 0


def read_ref_lines(stream, timeout):
    """Returns the ref lines from stream, or None if there are none.

    None means: stream is a terminal, nothing arrived within timeout seconds (the stream
    stays open without data) or the stream held no ref line at all."""
    if stream.isatty():
        return None
    collected = []

    def read_all():
        collected.append(stream.read())

    reader = threading.Thread(target=read_all)
    reader.daemon = True
    reader.start()
    reader.join(timeout)
    if reader.is_alive() or len(collected) == 0:
        return None
    ref_lines = [line for line in collected[0].split("\n") if line.strip() != ""]
    if len(ref_lines) == 0:
        return None
    return ref_lines


def main(argv):
    terms_path = None
    positional = []
    index = 1
    while index < len(argv):
        if argv[index] == "--self-test":
            return self_test()
        if argv[index] == "--terms" and index + 1 < len(argv):
            terms_path = argv[index + 1]
            index += 2
            continue
        positional.append(argv[index])
        index += 1
    remote_name = positional[0] if positional else "origin"

    cwd = os.getcwd()
    terms, error = find_terms(cwd, terms_path)
    if terms is None:
        sys.stderr.write("check-protected-content: %s\n" % error)
        return 1

    ref_lines = read_ref_lines(sys.stdin, STDIN_TIMEOUT_SECONDS)
    if ref_lines is None:
        sys.stderr.write("check-protected-content: %s\n" % NO_REF_LINES_MESSAGE)
        sys.stderr.flush()
        # the reader thread may still block on stdin, a normal exit could hang on it
        os._exit(1)
    problems = check_push(ref_lines, terms, remote_name, cwd)
    check_level_similarity(ref_lines, remote_name, cwd, problems)
    check_campaign_progression(ref_lines, remote_name, cwd, problems)
    if problems:
        return report_and_exit(problems)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
