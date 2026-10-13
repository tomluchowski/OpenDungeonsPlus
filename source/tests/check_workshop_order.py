"""Compile current scheduling/save blocks with simulated world services.

Run in the prepared compiler environment:
    python source/tests/check_workshop_order.py
Use --source-ref HEAD to reproduce the regression against committed sources.
This does not launch the game or simulate worker movement or delivery.
"""

import argparse
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--source-ref", help="Read production sources from a Git revision")
parser.add_argument("--compiler", default="cl", help="cl or a GCC/Clang-compatible compiler")
args = parser.parse_args()


def read_source(path):
    if args.source_ref:
        return subprocess.check_output(
            ["git", "show", f"{args.source_ref}:{path}"], cwd=ROOT, text=True
        )
    return (ROOT / path).read_text(encoding="utf-8")


workshop = read_source("source/rooms/RoomWorkshop.cpp")
upkeep = workshop[workshop.index("void RoomWorkshop::doUpkeep()") :]
schedule = upkeep[upkeep.index("    if(mTrapType ==") : upkeep.index("    // If there is nothing to do")]
map_handler = read_source("source/gamemap/MapHandler.cpp")
save = map_handler[map_handler.index("    std::vector<Trap*> traps = gameMap.getTraps();") :]
save = save[: save.index("    // Write out the lights")]

probe = r'''
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

enum class TrapType { nullTrapType, cannon, spike, boulder, doorWooden };
enum class GameEntityType { room, trap, craftedTrap };
struct Seat {};
struct Tile {};
struct Creature {};
struct Building {
    virtual ~Building() {}
    virtual GameEntityType getObjectType() const { return GameEntityType::room; }
};
struct Trap : Building {
    TrapType type;
    int needed;
    int id;
    Trap(TrapType t, int n, int i) : type(t), needed(n), id(i) {}
    GameEntityType getObjectType() const override { return GameEntityType::trap; }
    TrapType getType() const { return type; }
    int32_t getNbNeededCraftedTrap() const { return needed; }
    int numCoveredTiles() const { return 1; }
    static bool sortForMapSave(Trap* a, Trap* b) { return a->type < b->type; }
};
struct RenderedMovableEntity {
    Seat* seat;
    bool onMap;
    RenderedMovableEntity(Seat* s, bool m) : seat(s), onMap(m) {}
    virtual ~RenderedMovableEntity() {}
    virtual GameEntityType getObjectType() const { return GameEntityType::room; }
    Seat* getSeat() const { return seat; }
    bool getIsOnMap() const { return onMap; }
};
struct CraftedTrap : RenderedMovableEntity {
    TrapType type;
    CraftedTrap(Seat* s, TrapType t, bool m = true) : RenderedMovableEntity(s, m), type(t) {}
    GameEntityType getObjectType() const override { return GameEntityType::craftedTrap; }
    TrapType getTrapType() const { return type; }
};
struct GameMap {
    Creature worker;
    bool hasWorker = true;
    bool editor = false;
    std::vector<Building*> reachable;
    std::vector<RenderedMovableEntity*> stock;
    std::vector<Trap*> traps;
    Creature* getWorkerForPathFinding(Seat*) { return hasWorker ? &worker : nullptr; }
    std::vector<Building*> getReachableBuildingsPerSeat(Seat*, Tile*, Creature*) { return reachable; }
    const std::vector<RenderedMovableEntity*>& getRenderedMovableEntities() { return stock; }
    std::vector<Trap*> getTraps() { return traps; }
    bool isInEditorMode() { return editor; }
};
struct GameEntity {
    static void exportToStream(Trap* trap, std::ostream& os) { os << trap->id << '\n'; }
};
// Deterministic random choice exposes the old scheduler's loss of insertion order.
struct Random { static uint32_t Uint(uint32_t, uint32_t last) { return last; } };
struct Workshop {
    GameMap map;
    Seat seat;
    Tile tile;
    std::vector<Tile*> mCoveredTiles { &tile };
    TrapType mTrapType = TrapType::nullTrapType;
    GameMap* getGameMap() { return &map; }
    Seat* getSeat() { return &seat; }
    TrapType schedule() {
SCHEDULE
        return mTrapType;
    }
};
std::string saveTraps(GameMap& gameMap) {
    std::ostringstream levelFile;
SAVE
    return levelFile.str();
}
int failures = 0;
void check(bool condition, const char* name) {
    std::cout << (condition ? "PASS " : "FAIL ") << name << '\n';
    failures += !condition;
}
int main() {
    using T = TrapType;
    Workshop w;
    Trap first(T::cannon, 1, 1), second(T::doorWooden, 1, 2), third(T::cannon, 1, 3);
    Building room;
    w.map.reachable = { &room, &first, &second, &third };
    check(w.schedule() == T::cannon, "first order precedes other types");
    w.mTrapType = T::nullTrapType;
    CraftedTrap cannon(&w.seat, T::cannon);
    w.map.stock = { &cannon };
    check(w.schedule() == T::doorWooden, "stock covers oldest matching order only");
    w.mTrapType = T::nullTrapType;
    first.needed = 2;
    check(w.schedule() == T::cannon, "partially covered multi-tile order stays first");
    w.mTrapType = T::nullTrapType;
    first.needed = 0;
    w.map.stock.clear();
    check(w.schedule() == T::doorWooden, "completed or reserved order is skipped");
    w.mTrapType = T::nullTrapType;
    w.map.reachable = { &third };
    check(w.schedule() == T::cannon, "removed or unreachable order no longer blocks work");
    w.mTrapType = T::nullTrapType;
    Seat enemy;
    CraftedTrap otherSeat(&enemy, T::cannon), carried(&w.seat, T::cannon, false);
    RenderedMovableEntity unrelated(&w.seat, true);
    w.map.stock = { &otherSeat, &carried, &unrelated };
    check(w.schedule() == T::cannon, "foreign carried and non-trap stock excluded");
    w.mTrapType = T::nullTrapType;
    w.map.stock = { &cannon };
    check(w.schedule() == T::nullTrapType, "fully supplied orders start no production");
    w.mTrapType = T::boulder;
    check(w.schedule() == T::boulder, "in-progress work is preserved");
    w.mTrapType = T::nullTrapType;
    w.map.hasWorker = false;
    w.map.stock.clear();
    check(w.schedule() == T::nullTrapType, "no pathfinding worker starts no work");
    w.map.hasWorker = true;
    w.map.reachable.clear();
    check(w.schedule() == T::nullTrapType, "empty order list starts no work");
    w.map.traps = { &second, &first, &third };
    const std::string gameSave = saveTraps(w.map);
    check(gameSave.find("\n2\n") < gameSave.find("\n1\n") &&
          gameSave.find("\n1\n") < gameSave.find("\n3\n"),
          "gameplay serialization preserves order across types");
    w.map.editor = true;
    const std::string editorSave = saveTraps(w.map);
    check(editorSave.find("\n1\n") < editorSave.find("\n2\n") &&
          editorSave.find("\n3\n") < editorSave.find("\n2\n"),
          "editor serialization retains type sorting");
    check(w.map.traps.front() == &second, "serialization does not reorder the live map");
    return failures ? 1 : 0;
}
'''.replace("SCHEDULE", schedule).replace("SAVE", save)
# The save header is descriptive only; use the test's minimal Trap representation.
probe = probe.replace('Trap::getTrapStreamFormat()', '"probe trap IDs"')

with tempfile.TemporaryDirectory(prefix="workshop-order-") as directory:
    path = Path(directory)
    source = path / "check.cpp"
    source.write_text(probe, encoding="utf-8")
    executable = path / "check.exe"
    if Path(args.compiler).stem.lower() == "cl":
        command = [args.compiler, "/nologo", "/EHsc", "/std:c++14", str(source), f"/Fe:{executable}"]
    else:
        command = [args.compiler, "-std=c++14", str(source), "-o", str(executable)]
    subprocess.run(command, cwd=path, check=True)
    result = subprocess.run([str(executable)], cwd=path)
    raise SystemExit(result.returncode)
