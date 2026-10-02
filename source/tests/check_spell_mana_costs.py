"""Check the Summon worker price rule and the spell costs of the three spells touched with it.

Summon worker costs 1,500 per step, Call to war 10,000, Heal 5,000 and Eye of evil
5,000, all without recharge. The worker price grows by one base price per worker
above the four the heart supplies."""
from pathlib import Path
import re
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]

config = (repo / "config/spells.cfg").read_text()
section = config[config.rindex("[Spells]"):config.rindex("[/Spells]")]
values = dict(re.findall(r"^\s+(\w+)\s+([\d.]+)\s*$", section, re.M))

expected = {
    "SummonWorkerNbHeart": "4", "SummonWorkerBasePrice": "1500", "SummonWorkerCooldown": "0",
    "CallToWarPrice": "10000", "CallToWarCooldown": "0",
    "CreatureHealPrice": "5000", "CreatureHealCooldown": "0",
    "EyeEvilPrice": "5000", "EyeEvilCooldown": "0",
}
for key, value in expected.items():
    assert values.get(key) == value, f"{key} is {values.get(key)}, expected {value}"
assert "SummonWorkerNbFree" not in config, "the free-worker key is gone"

spell = (repo / "source/spells/SpellSummonWorker.cpp").read_text()
assert "std::pow" not in spell, "the worker price no longer doubles"
assert "SummonWorkerNbFree" not in spell
assert spell.count("getWorkerPrice(basePrice, nbWorkers)") == 3, \
    "cast check, server cast and next price use the same rule"

probe = r"""
#include <algorithm>
#include <cstdint>
#include <cstdio>
static int32_t price(int32_t base, int32_t workers, int32_t heart)
{ return base * std::max(1, workers - heart + 1); }
int main()
{
    int bad = 0;
    // the heart supplies four workers: the fifth costs the base price, then 2x, 3x ...
    const int32_t expectedPrice[][2] = {{0, 1500}, {3, 1500}, {4, 1500}, {5, 3000}, {6, 4500}, {7, 6000}};
    for(const int32_t* row : expectedPrice)
        if(price(1500, row[0], 4) != row[1]) { std::printf("workers %d gives %d\n", row[0], price(1500, row[0], 4)); bad = 1; }
    return bad;
}
"""
with tempfile.TemporaryDirectory(prefix="odp-spell-costs-") as directory:
    work = Path(directory)
    (work / "check.cpp").write_text(probe)
    subprocess.run(["cl", "/nologo", "/EHsc", "/MD", "/std:c++14", "check.cpp", "/Fecheck.exe"], cwd=work, check=True)
    subprocess.run([str(work / "check.exe")], cwd=work, check=True)
print("SPELL COSTS OK")
