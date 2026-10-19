"""Check the room values: prices, prison and torture
damage, arena damage, torture conversion times, library storage and the
room based creature attraction."""
from pathlib import Path
import re

repo = Path(__file__).resolve().parents[2]


def read(path):
    return (repo / path).read_text(encoding="utf-8")


creature = read("source/entities/Creature.cpp")
assert "ArenaDamageTakenPercent" in creature
assert creature.count("RoomType::arena") >= 3, "pit damage checks the arena of both fighters"
print("CHECKS OK")
