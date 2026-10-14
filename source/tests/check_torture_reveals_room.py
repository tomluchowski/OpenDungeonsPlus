"""Check that a tortured enemy creature that dies gives away one not yet seen room
of its owner instead of a timed disc around the dungeon heart."""
from pathlib import Path

repo = Path(__file__).resolve().parents[2]


def read(path):
    return (repo / path).read_text(encoding="utf-8")


rooms = read("config/rooms.cfg")
assert "TortureRevealRadius" not in rooms and "TortureRevealTurns" not in rooms
torture = read("source/rooms/RoomTorture.cpp")
assert "TortureRevealRadius" not in torture and "TortureRevealTurns" not in torture
body = torture[torture.index("void RoomTorture::revealEnemyInformation"):]
body = body[:body.index("bool RoomTorture::useRoom")]
assert "room->getSeat() != creature.getSeat()" in body
assert "hasSeenTile(firstTile)" in body
assert "getCoveredTiles()" in body
assert "revealTiles(roomToReveal->getCoveredTiles(), 1)" in body
assert "dungeonTemple" not in body
assert "if(creature->getHP() <= damage)" in torture
print("CHECKS OK")
