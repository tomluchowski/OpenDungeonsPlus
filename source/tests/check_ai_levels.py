"""Check the numbers of the three AI levels (easy, normal, hard): attack with more than
14 / 11 / 8 creatures, minimum creature level 2 / 4 / 7 and retreat at 25 / 20 / 12 percent
health, and that the harder level is never the more cautious one."""
from pathlib import Path
import re

repo = Path(__file__).resolve().parents[2]
src = (repo / "source/ai/AIFactory.cpp").read_text(encoding="utf-8")

expected = {
    "easy": ("150", "25", "14", "2"),
    "normal": ("100", "20", "11", "4"),
    "hard": ("60", "12", "8", "7"),
}
for level, (reaction, retreat, attack, minLevel) in expected.items():
    m = re.search(r"case KeeperAIType::%s:\s*return new KeeperAI\(([^;]*)\);" % level, src)
    assert m, f"no KeeperAI for level {level}"
    args = [a.strip() for a in m.group(1).split(",")]
    # gameMap, player, 6 cooldowns, reaction, retreat HP percent, attack threshold, min level, traps, doors
    assert len(args) == 14, f"{level}: unexpected argument count {len(args)}"
    assert args[8:12] == [reaction, retreat, attack, minLevel], \
        f"{level}: reaction/retreat/attack is {args[8:12]}, expected {[reaction, retreat, attack, minLevel]}"

order = [expected[level] for level in ("easy", "normal", "hard")]
for column, descending in ((0, True), (1, True), (2, True), (3, False)):
    values = [int(row[column]) for row in order]
    assert values == sorted(values, reverse=descending), f"difficulty ordering broken in column {column}: {values}"

doc = (repo / "docs/development/AI-LEVELS.md").read_text(encoding="utf-8")
assert "## Not built" in doc
print("OK")
