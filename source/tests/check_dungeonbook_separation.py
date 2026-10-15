"""Runs scripts/check_dungeonbook_separation.py, so the check is part of the source/tests/check_*.py set.

The check itself lives in scripts/ because the creature picture plan names it there; this wrapper
only makes sure the build's check run (which globs source/tests/check_*.py) executes it as well.
"""
from pathlib import Path
import subprocess
import sys

script = Path(__file__).resolve().parents[2] / 'scripts' / 'check_dungeonbook_separation.py'
assert script.exists(), script
result = subprocess.run([sys.executable, str(script)])
sys.exit(result.returncode)
