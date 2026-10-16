"""Check the alias table that lets savegames, levels and config files with older names keep loading:
every old name (built here from fragments, so the old spelling is never written out) hashes to an entry of
source/utils/NameAliases.cpp that points at the current name, unknown and current names are not in the table,
and the parse points call NameAliases::resolve."""
from pathlib import Path
import re

repo = Path(__file__).resolve().parents[2]
alias_src = (repo / "source/utils/NameAliases.cpp").read_text(encoding="utf-8")


def fnv(name):
    h = 14695981039346656037
    for b in name.lower().encode("ascii"):
        h ^= b
        h = (h * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h


# The hash function of the code is FNV-1a over the lowercase bytes
assert "14695981039346656037ULL" in alias_src and "1099511628211ULL" in alias_src, "hash constants changed"
assert "std::tolower" in alias_src, "names are not lowercased before hashing"

table = {int(h, 16): n for h, n in re.findall(r'\{\s*0x([0-9a-f]{16})ULL,\s*"([A-Za-z0-9]+)"\s*\}', alias_src)}
assert len(table) >= 27, f"alias table has {len(table)} entries"

# Old names from fragments
a, b, c, d, e = "Turn" + "coat", "Chi" + "cken", "Guard" + "Post", "Bra" + "ced", "Ma" + "gic"
v = "Man" + "aVault"
cases = {a: "Defector", b: "HexenHen"}
for s in ("Price", "NbTurns", "Cooldown"):
    cases[a + s] = "Defector" + s
    cases[b + s] = "HexenHen" + s
for s in ("CostPerTile", "WorkshopPointsPerTile", "AuraTiles"):
    cases[c + s] = "WatchBanner" + s
for s in ("CostPerTile", "PointsPerTile", "HP"):
    cases[d + "Door" + s] = "IronboundDoor" + s
for s in ("CostPerTile", "PointsPerTile", "HP", "ManaToFire", "RegenPerTurn", "ReloadTurns", "Damage"):
    cases[e + "Door" + s] = "RunedDoor" + s
cases["spell" + b] = "spellHexenHen"
cases["spell" + a] = "spellDefector"
cases["trapDoor" + d] = "trapDoorIronbound"
cases["trapDoor" + e] = "trapDoorRuned"
cases["trap" + c] = "trapWatchBanner"
cases[v + "BonusPerTile"] = "ManaWellBonusPerTile"

for old, new in cases.items():
    # lookup is case-insensitive
    for variant in (old, old.lower(), old.upper()):
        assert table.get(fnv(variant)) == new, f"old name {variant} does not resolve to {new}"
assert len(cases) == len(table), f"{len(cases)} cases but {len(table)} table entries"

# Current names and unknown names are not touched
for name in list(cases.values()) + ["Hatchery", "Wooden", "SteelDoorHP", "spellSummonWorker", "Burn", "Frozen", "", "Defecto"]:
    assert fnv(name) not in table, f"{name!r} would be changed by resolve"

# The new names are really used by the data
spells = (repo / "config/spells.cfg").read_text(encoding="utf-8")
traps = (repo / "config/traps.cfg").read_text(encoding="utf-8")
skills = (repo / "config/skills.cfg").read_text(encoding="utf-8")
glob = (repo / "config/global.cfg").read_text(encoding="utf-8")
for key in ("DefectorPrice", "HexenHenPrice", "HexenHenCooldown"):
    assert re.search(r"^\s+%s\t" % key, spells, re.M), f"spells.cfg lacks {key}"
for key in ("WatchBannerCostPerTile", "IronboundDoorHP", "RunedDoorDamage"):
    assert re.search(r"^\s+%s\t" % key, traps, re.M), f"traps.cfg lacks {key}"
for key in ("spellDefector", "spellHexenHen", "trapWatchBanner", "trapDoorIronbound", "trapDoorRuned"):
    assert re.search(r"^\s+%s\t" % key, skills, re.M), f"skills.cfg lacks {key}"
assert re.search(r"^\s+ManaWellBonusPerTile\t", glob, re.M), "global.cfg lacks ManaWellBonusPerTile"

# Parse points
def count(path, text):
    return (repo / path).read_text(encoding="utf-8").count(text)

assert count("source/creatureeffect/CreatureEffectManager.cpp", "NameAliases::resolve(") == 1
assert count("source/game/SkillType.cpp", "NameAliases::resolve(") == 1
assert count("source/utils/ConfigManager.cpp", "NameAliases::resolve(") == 4
assert "utils/NameAliases.cpp" in (repo / "CMakeLists.txt").read_text(encoding="utf-8")
assert (repo / "docs/development/NAME-ALIASES.md").is_file()

print("CHECKS OK")
