"""Check that the temple sacrifice works as documented: no mana and no research points
for a sacrifice, a queue of the last three sacrifices, the first matching recipe wins and
the level of a new creature is the rounded down average of the inputs."""
from pathlib import Path
import itertools
import re

repo = Path(__file__).resolve().parents[2]


def read(path):
    return (repo / path).read_text(encoding="utf-8").replace("\r\n", "\n")


rooms = read("config/rooms.cfg")
temple = read("source/rooms/RoomTemple.cpp")
header = read("source/rooms/RoomTemple.h")

for key in ("TempleSacrificeManaPerLevel", "TempleSacrificeWaitTurns", "TempleManaBoost"):
    assert key not in rooms and key not in temple, f"{key} must be gone"
assert not re.search(r"Temple\w*Research", rooms + temple), "no research points for sacrifices"

sacrifice = temple[temple.index("void RoomTemple::sacrificeCreature"):temple.index("void RoomTemple::giveSacrificeResult")]
assert "addManaToSeat" not in sacrifice, "a sacrifice gives no mana"
assert "TEMPLE_SACRIFICE_QUEUE_SIZE = 3" in temple and "mSacrificed.erase(mSacrificed.begin())" in sacrifice
assert "totalLevel / static_cast<uint32_t>(inputs.size())" in sacrifice and "+ 1" not in sacrifice.split("averageLevel")[1][:40]
assert "mTurnsSinceSacrifice" not in temple + header

values = dict(re.findall(r"^\s+(TempleRecipe\w+)\s+(\S+)\s*$", rooms, re.M))
count = int(values["TempleRecipeCount"])
creatures = set(re.findall(r"^\s+Name\s+(\w+)\s*$", read("config/creatures.cfg"), re.M))
for i in range(1, count + 1):
    recipe = values[f"TempleRecipe{i}"]
    inputs, result = recipe.split("=")
    assert 1 <= len(inputs.split("+")) <= 3, recipe
    for name in inputs.split("+"):
        assert name in creatures, f"{recipe}: unknown creature {name}"
    assert result in ("ManaBoost", "Workers") or result in creatures, recipe
assert values["TempleRecipe1"] == "Elf+Elf=Orc"
assert values["TempleRecipe2"] == "Gnome+Gnome+Gnome=ManaBoost"
assert re.search(r"^\s+TempleWorkersGiven\s+10\s*$", rooms, re.M) and "TempleWorkersGiven" in temple

# The recipes that worked before the queue (12 creature and special recipes, the ones with
# three inputs in any order) must all still resolve to the same result in the queue model:
# the last three sacrifices, the first recipe that matches the newest ones wins.
old_recipes = [
    "Lich+Lich=PitDemon", "Troll+Troll=Cultist", "Orc+Cultist+CaveHornet=Workers",
    "LavaSpawn+LavaSpawn=LizardMan", "Elf+Elf=Orc", "Cultist+Cultist=Goblin",
    "Rat+Rat=LavaSpawn", "Skeleton+Skeleton=DarkElf", "Orc+Orc=Lich", "PitDemon+PitDemon=Rat",
    "LizardMan+LizardMan=Skeleton", "Gnome+Gnome+Gnome=ManaBoost", "PitDemon+DarkElf+Cultist=Workers",
]
table = [values[f"TempleRecipe{i}"].split("=") for i in range(1, count + 1)]
table = [(inputs.split("+"), result) for inputs, result in table]


def sacrifice(sequence):
    queue = []
    for name in sequence:
        queue = (queue + [name])[-3:]
        for inputs, result in table:
            if len(inputs) <= len(queue) and queue[len(queue) - len(inputs):] == inputs:
                queue = []
                break
        else:
            result = None
    return result


for recipe in old_recipes:
    inputs, result = recipe.split("=")
    for order in set(itertools.permutations(inputs.split("+"))):
        assert sacrifice(order) == result, f"{recipe} does not resolve for the order {order}"
# a sacrifice that matches nothing gives nothing, a leftover does not hide the next recipe
assert sacrifice(["Rat"]) is None and sacrifice(["Orc", "Rat", "Rat"]) == "LavaSpawn"

assert (repo / "docs/development/TEMPLE-SACRIFICE.md").exists()
print("CHECKS OK")
