"""Check that the Reveal Map special reveals the map for the rest of the game, not for a number of turns."""
from pathlib import Path

repo = Path(__file__).resolve().parents[2]
seat = (repo / "source/game/Seat.h").read_text(encoding="utf-8")
bonus = (repo / "source/giftboxes/GiftBoxBonus.cpp").read_text(encoding="utf-8")
game_map = (repo / "source/gamemap/GameMap.cpp").read_text(encoding="utf-8")

assert "revealMapPermanently" in seat and "isMapRevealed" in seat
assert "mRevealMapTurns" not in seat and "consumeRevealMapTurn" not in seat
assert "seat->revealMapPermanently();" in bonus
assert "seat->isMapRevealed()" in game_map
print("OK")
