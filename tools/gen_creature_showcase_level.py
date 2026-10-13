#!/usr/bin/env python3
"""Writes levels/skirmish/TestCreatureShowcase.level (run from the repository root).

A test map for the creature reactions and the room effects: one claimed 7x7 hall per room type
(the portal takes 3x3 in the middle), corridors between them, and in every hall one creature of
every creature type from config/creatures.cfg, so that every type stands next to every room.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "levels" / "skirmish" / "TestCreatureShowcase.level"

COLUMNS = 5
CELL = 9
HALL = 7
ORIGIN = 2

# (room type number, name prefix, size). Numbers follow RoomType in source/rooms/RoomType.h.
HALLS = [
    (1, "DungeonTemple", 5), (2, "Dormitory", 7), (3, "Treasury", 7), (4, "Portal", 3), (5, "Workshop", 7),
    (6, "TrainingHall", 7), (7, "Library", 7), (8, "Hatchery", 7), (9, "Crypt", 7), (11, "Prison", 7),
    (14, "Arena", 7), (15, "Casino", 7), (16, "Torture", 7), (17, "GuardRoom", 7), (18, "Temple", 7),
]

# Data a room type reads after its tiles (beds, claimed value, points, held creatures).
ROOM_EXTRA = {2: "0", 4: "9\t5", 5: "0\t0", 7: "0", 9: "0", 11: "0", 16: "0"}

SKILLS = [
    "roomTreasury", "roomDormitory", "roomHatchery", "roomLibrary", "spellSummonWorker", "roomArena",
    "roomBridgeStone", "roomBridgeWooden", "roomCasino", "roomCrypt", "roomPrison", "roomTorture",
    "roomTrainingHall", "roomWorkshop", "roomGuardRoom", "roomTemple", "trapCannon", "trapBoulder",
    "trapSpike", "trapDoorWooden", "spellCallToWar", "spellCreatureDefense", "spellCreatureExplosion",
    "spellCreatureHaste", "spellCreatureHeal", "spellCreatureSlow", "spellCreatureStrength",
    "spellCreatureWeak", "spellEyeEvil",
]


def read_creatures():
    text = (ROOT / "config" / "creatures.cfg").read_text(encoding="utf-8")
    result = []
    for block in re.findall(r"\[Creature\](.*?)\[/Creature\]", text, re.S):
        def value(key):
            m = re.search(r"^\s*" + key + r"\s+(\S+)", block, re.M)
            return m.group(1) if m else None
        name = value("Name")
        mesh = value("MeshName")
        if name is None or mesh is None:
            continue
        result.append((name, mesh, value("WeaponSpawnL") or "none", value("WeaponSpawnR") or "none"))
    return result


def main():
    creatures = read_creatures()
    if not creatures:
        sys.exit("no creatures found in config/creatures.cfg")
    rows = (len(HALLS) + COLUMNS - 1) // COLUMNS
    line_y = ORIGIN + rows * CELL + 1
    size_x = ORIGIN + COLUMNS * CELL + 2
    size_y = line_y + 4
    floor = set()
    halls = []
    for index, (number, prefix, size) in enumerate(HALLS):
        col, row = index % COLUMNS, index // COLUMNS
        x0 = ORIGIN + col * CELL + (HALL - size) // 2
        y0 = ORIGIN + row * CELL + (HALL - size) // 2
        tiles = [(x, y) for x in range(x0, x0 + size) for y in range(y0, y0 + size)]
        # The whole hall is floor; the room covers its middle (all of it, except for the portal)
        hall_x, hall_y = ORIGIN + col * CELL, ORIGIN + row * CELL
        floor.update((x, y) for x in range(hall_x, hall_x + HALL) for y in range(hall_y, hall_y + HALL))
        halls.append((number, "%s%d" % (prefix, index + 1), tiles, hall_x, hall_y, size))
        # Corridor to the right neighbour and to the row below (or to the line-up hall).
        cell_x, cell_y = ORIGIN + col * CELL, ORIGIN + row * CELL
        if col + 1 < COLUMNS and index + 1 < len(HALLS):
            for x in range(cell_x + HALL, cell_x + CELL):
                floor.add((x, cell_y + HALL // 2))
        for y in range(cell_y + HALL, (cell_y + CELL) if row + 1 < rows else line_y):
            floor.add((cell_x + HALL // 2, y))
    for x in range(ORIGIN, size_x - 2):
        floor.add((x, line_y))
    heart = halls[0]
    out = []
    out.append("OpenDungeons_Version:0.7.1  # The version of OpenDungeons which created this file (for compatibility reasons).\n")
    out.append("[Info]\nName\t\\[Test\\] Creature showcase\n"
               "Description\tOne hall per room type and a line-up of every creature type, for checking creature reactions and room effects.\n"
               "Music\tSearching_yd.ogg\nFightMusic\tTheDarkAmulet_MP.ogg\nSandbox\t1\n[/Info]\n")
    out.append("[Seats]\n[Seat]\nseatId\t1\nteamId\t1\nplayer\tHuman\nfaction\tKeeper\nstartingX\t%d\nstartingY\t%d\n"
               "colorId\t1\ngold\t5000\ngoldMined\t100\nmana\t100\n[SkillDone]\n%s\n[/SkillDone]\n[SkillNotAllowed]\n"
               "[/SkillNotAllowed]\n[SkillPending]\n[/SkillPending]\n[/Seat]\n[/Seats]\n"
               % (heart[3] + 2, heart[4] + 2, "\n".join(SKILLS)))
    out.append("[Goals]\n# goalName\targuments\n[/Goals]\n")
    out.append("[Tiles]\n# Map Size\n%d # MapSizeX\n%d # MapSizeY\n# posX\tposY\ttype\tfullness\tseatId(optional)\n" % (size_x, size_y))
    for x, y in sorted(floor):
        out.append("%d\t%d\t1\t0\t1\n" % (x, y))
    # Solid rock around the halls: workers have no wall to claim, so nobody walks out of a hall for it.
    for x in range(size_x):
        for y in range(size_y):
            if (x, y) not in floor:
                out.append("%d\t%d\t2\t100\n" % (x, y))
    out.append("[/Tiles]\n")
    out.append("[Rooms]\n# typeRoom\tname\tseatId\tnumTiles\t\tSubsequent Lines: tileX\ttileY\n")
    for number, name, tiles, _x0, _y0, _size in halls:
        out.append("[Room]\n%d\t%s\t1\t%d\n" % (number, name, len(tiles)))
        for x, y in tiles:
            out.append("%d\t%d\n" % (x, y))
        if number in ROOM_EXTRA:
            out.append(ROOM_EXTRA[number] + "\n")
        out.append("[/Room]\n")
    out.append("[/Rooms]\n")
    out.append("[Traps]\n# typeTrap\tname\tseatId\tnumTiles\t\tSubsequent Lines: tileX\ttileY\tisActivated(0/1)\t\tSubsequent Lines: optional specific data\n[/Traps]\n")
    out.append("[Lights]\n# posX\tposY\tposZ\tdiffuseR\tdiffuseG\tdiffuseB\tspecularR\tspecularG\tspecularB\tattenRange\tattenConst\tattenLin\tattenQuad\n[/Lights]\n")
    out.append("[CreatureDefinitions]\n[/CreatureDefinitions]\n[EquipmentDefinitions]\n[/EquipmentDefinitions]\n")
    out.append("[Creatures]\n# SeatId\tName\tMeshName\tPosX\tPosY\tPosZ\tClassName\tLevel\tCurrentXP\tCurrentHP\tCurrentWakefulness\t"
               "CurrentHunger\tGoldToDeposit\tLeftWeapon\tRightWeapon\tCarriedSkill\tCarriedWeapon\tNbCreatureEffects\tN*CreatureEffects\n")
    counter = [0]

    def creature(entry, x, y):
        counter[0] += 1
        name, mesh, left, right = entry
        out.append("1\t%s%d\t%s\t%d\t%d\t0\t%s\t1\t0\tmax\t100\t0\t0\t%s\t%s\tnullSkillType\tnone\t0\n"
                   % (name, counter[0], mesh, x, y, name, left, right))

    # Every hall (the heart hall too): one creature of every type, one per tile, on the floor around the
    # room first and then on the room
    for _number, _name, tiles, hall_x, hall_y, _size in halls:
        room = set(tiles)
        spots = [(x, y) for y in range(hall_y, hall_y + HALL) for x in range(hall_x, hall_x + HALL)]
        spots = [spot for spot in spots if spot not in room] + [spot for spot in spots if spot in room]
        for entry, (x, y) in zip(creatures, spots):
            creature(entry, x, y)
    out.append("[/Creatures]\n")
    for section, header in (
            ("Spells", "# typeSpell\tSeatId\tName\tMeshName\tPosX\tPosY\tPosZ\topacity\trotationAngle\toptionalData"),
            ("CraftedTraps", "# SeatId\tName\tMeshName\tPosX\tPosY\tPosZ\topacity\trotationAngle\ttrapType\tPosX\tPosY\tPosZ"),
            ("SkillEntity", "# SeatId\tName\tMeshName\tPosX\tPosY\tPosZ\topacity\trotationAngle\tskillPoints\tPosX\tPosY\tPosZ"),
            ("GiftBoxEntity", "# GiftBoxType\tSeatId\tName\tMeshName\tPosX\tPosY\tPosZ\topacity\trotationAngle\toptionalData"),
            ("Missiles", "# missileType\tSeatId\tName\tMeshName\tPosX\tPosY\tPosZ\topacity\trotationAngle\tdirectionX\tdirectionY\tdirectionZ\tmissileAlive\tdamageAllies\tspeed\toptionalData"),
            ("TreasuryObject", "# SeatId\tName\tMeshName\tPosX\tPosY\tPosZ\topacity\trotationAngle\tvalue"),
            ("Chickens", "# SeatId\tName\tMeshName\tPosX\tPosY\tPosZ\topacity\trotationAngle\tPosX\tPosY\tPosZ")):
        out.append("[%s]\n%s\n[/%s]\n" % (section, header, section))
    OUT.write_text("".join(out), encoding="utf-8", newline="\n")
    print("wrote %s: %d floor tiles, %d creatures" % (OUT, len(floor), counter[0]))


main()
